//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#ifdef __APPLE__

#include "umc_defs.h"
#include "umc_va_base.h"

#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef UMC_RESTRICTED_CODE_VA
#ifdef UMC_VA_OSX

#include <algorithm>
#include <vector>
#include <queue>

#include <VTDecompressionSession.h> //For Apple Video Toolbox framework
#include <CoreMedia/CMTime.h>
#include <dlfcn.h>

#include "umc_h264_vda_supplier.h"
#include "umc_h264_frame_list.h"
#include "umc_automatic_mutex.h"
#include "umc_h264_nal_spl.h"
#include "umc_h264_bitstream.h"

#include "umc_h264_dec_defs_dec.h"
#include "vm_sys_info.h"
#include "umc_h264_segment_decoder_mt.h"

#include "umc_h264_task_broker.h"
#include "umc_video_processing.h"
#include "umc_structures.h"

#include "umc_h264_dec_debug.h"

#ifdef USE_APPLEGVA
#include <CGDirectDisplay.h>    //Needed to add CoreGraphics to builder/FindPackages.cmake
#include <CoreVideo/CVPixelBuffer.h>
#include <iostream>
#include <string>

class TaskBrokerSingleThreadVDA;
#endif

#ifndef ERROR_FRAME_NONE
#define ERROR_FRAME_NONE 0
#endif

#ifndef ERROR_FRAME_MINOR
#define ERROR_FRAME_MINOR 1
#endif

#ifndef ERROR_FRAME_MAJOR
#define ERROR_FRAME_MAJOR 1
#endif

//#define TRY_PROPERMEMORYMANAGEMENT

//define PRINT_HISTOGRAM to have the decoder output callback print a histogram of y, u, and v.
//#define PRINT_HISTOGRAM


#define RMH_USE_ADDTONALQUEUE
#define USE_MEMCPY
#include <ippi.h>

//#define DEBUG_VTB
#ifdef DEBUG_VTB
#define DEBUG_VTB_PRINT(desired)\
desired

#else
#define DEBUG_VTB_PRINT(desired)
#endif

#ifdef VTB_LOAD_LIB_AT_INIT
#define CALL_VTB_FUNC(theFunction)\
call_##theFunction
#else
#define CALL_VTB_FUNC(theFunction)\
theFunction
#endif  //ifdef VTB_LOAD_LIB_AT_INIT

namespace UMC
{
    
const char * getVTErrorString(OSStatus errorNumber)
{
    typedef struct _videoToolboxErrors  {
        const char * pErrorString;
        OSStatus errorValue;
    } vidoeToolboxErrors;
    
    //From CoreServices.CarbonCore Framework MacErrors.h and Video Toolbox Framework header file VTErrors.h
    vidoeToolboxErrors vtErrors[] = {
        
        //From MacErrors.h (CoreServices.CarbonCore Framework)
        {"codecErr", -8960},
        {"noCodecErr", -8961},
        {"codecUnimpErr", -8962},
        {"codecSizeErr", -8963},
        {"codecScreenBufErr", -8964},
        {"codecImageBufErr", -8965},
        {"codecSpoolErr", -8966},
        {"codecAbortErr", -8967},
        {"codecWouldOffscreenErr", -8968},
        {"codecBadDataErr", -8969},
        {"codecDataVersErr", -8970},
        {"codecExtensionNotFoundErr", -8971},
        {"scTypeNotFoundErr", codecExtensionNotFoundErr},
        {"codecConditionErr", -8972},
        {"codecOpenErr", -8973},
        {"codecCantWhenErr", -8974},
        {"codecCantQueueErr", -8975},
        {"codecNothingToBlitErr", -8976},
        {"codecNoMemoryPleaseWaitErr", -8977},
        {"codecDisabledErr", -8978}, /* codec disabled itself -- pass codecFlagReenable to reset*/
        {"codecNeedToFlushChainErr", -8979},
        {"lockPortBitsBadSurfaceErr", -8980},
        {"lockPortBitsWindowMovedErr", -8981},
        {"lockPortBitsWindowResizedErr", -8982},
        {"lockPortBitsWindowClippedErr", -8983},
        {"lockPortBitsBadPortErr", -8984},
        {"lockPortBitsSurfaceLostErr", -8985},
        {"codecParameterDialogConfirm", -8986},
        {"codecNeedAccessKeyErr", -8987}, /* codec needs password in order to decompress*/
        {"codecOffscreenFailedErr", -8988},
        {"codecDroppedFrameErr", -8989}, /* returned from ImageCodecDrawBand */
        {"directXObjectAlreadyExists", -8990},
        {"lockPortBitsWrongGDeviceErr", -8991},
        {"codecOffscreenFailedPleaseRetryErr", -8992},
        {"badCodecCharacterizationErr", -8993},
        {"noThumbnailFoundErr", -8994},
        
        //From Video Toolbox Framework header file VTErrors.h        
        {"kVTPropertyNotSupportedErr", -12900},
        {"kVTPropertyReadOnlyErr", -12901},
        {"kVTParameterErr", -12902},
        {"kVTInvalidSessionErr", -12903},
        {"kVTAllocationFailedErr", -12904},
        {"kVTPixelTransferNotSupportedErr", -12905}, // c.f. -8961
        {"kVTCouldNotFindVideoDecoderErr", -12906},
        {"kVTCouldNotCreateInstanceErr", -12907},
        {"kVTCouldNotFindVideoEncoderErr", -12908},
        {"kVTVideoDecoderBadDataErr", -12909}, // c.f. -8969
        {"kVTVideoDecoderUnsupportedDataFormatErr", -12910}, // c.f. -8970
        {"kVTVideoDecoderMalfunctionErr", -12911}, // c.f. -8960
        {"kVTVideoEncoderMalfunctionErr", -12912},
        {"kVTVideoDecoderNotAvailableNowErr", -12913},
        {"kVTImageRotationNotSupportedErr", -12914},
        {"kVTVideoEncoderNotAvailableNowErr", -12915},
        {"kVTFormatDescriptionChangeNotSupportedErr", -12916},
        {"kVTInsufficientSourceColorDataErr", -12917},
        {"kVTCouldNotCreateColorCorrectionDataErr", -12918},
        {"kVTColorSyncTransformConvertFailedErr", -12919},
        {"kVTVideoDecoderAuthorizationErr", -12210},
        {"kVTVideoEncoderAuthorizationErr", -12211},
        {"kVTColorCorrectionPixelTransferFailedErr", -12212},
        {"noErr", noErr},
        {"unknown error", 0}
    };
    
    unsigned int stringCount = sizeof(vtErrors)/sizeof(vidoeToolboxErrors) - 1;
    unsigned int index;
    for(index = 0; index < stringCount; index++)
    {
        if(vtErrors[index].errorValue == errorNumber)
        {
            break;
        }
    }
    
    return vtErrors[index].pErrorString;
}

//VDATaskSupplier Definitions
    
#ifdef USE_APPLEGVA
    
void printNALData(unsigned char *pNALData, unsigned int count)
{
    
    if (!pNALData) {
        printf(" Pointer to encoded NAL data is NULL\n");
        return;
    }
    
    printf(" printing %d bytes of the encoded NAL\n", count);
    for(unsigned int index = 0; index < count; index+=16)
    {
        printf("  Byte %4d:", index);
        for(unsigned int rowIndex = 0; (rowIndex < 16) && (rowIndex + index < count); rowIndex++)
        {
            printf(" 0x%02x", pNALData[index + rowIndex]);
        }
        printf("\n");
    }
}
    
void VDATaskSupplier::initializeAppleGVAMembers(void)
{
    m_pDisposeVideoBitstreamPool = (int (*) (void *, uint32_t)) NULL;
    m_pDestroyAccelerator = (int (*) (void *)) NULL;
    m_pSetDecodeDescription = (int (*) (void *, uint32_t, uint32_t, uint32_t, void *)) NULL;
    m_pCreateMainVideoDataPath = (int (*) (void *, uint32_t, struct _prepareStructure *)) NULL;
    m_pCreateVideoBitstreamPool = (int (*) (void *, uint32_t, uint32_t *)) NULL;
    m_pSetScanMode = (int (*) (void *, uint64_t, uint32_t *)) NULL;
    m_pQueryRenderMode = (int (*) (void *, uint64_t, uint32_t *)) NULL;
    
    m_pGetNALBufferAddress = (int (*) (void *, uint32_t, uint32_t, unsigned char * *)) NULL;
    m_pSetNALBufferReady = (int (*) (void *, uint32_t, uint32_t)) NULL;
    m_pSetNALDescriptors = (int (*) (void *, uint32_t, uint32_t, uint32_t, NALDescriptor * *)) NULL;
    m_pFunction15 = (int (*) (void *, uint32_t, uint32_t, NALDescriptor * *)) NULL;
    m_pGetDecoderStatistics = (int (*) (void *, uint32_t, uint32_t, uint32_t, uint32_t *)) NULL;
    
    bzero(&m_gvaInformation, sizeof(struct _gvaInformation));
    
    m_bVideoBitstreamPoolExists = false;
}
    
void VDATaskSupplier::releaseNALDescriptorPool(void)
{
    if (NULL == m_NALDescriptorPool) {
        return;
    }
    for (unsigned int index = 0; index < m_sizeNALDescriptorPool; index++) {
        
        if (m_NALDescriptorPool[index]) {
            free(m_NALDescriptorPool[index]);
        }
    }
    free(m_NALDescriptorPool);
}
#endif //USE_APPLEGVA
    
#ifdef VTB_LOAD_LIB_AT_INIT
VDATaskSupplier::VDATaskSupplier() : m_bufferedFrameNumber(0), m_sizeField(0), m_pFieldFrame(NULL), m_libHandle(NULL)
#else
    VDATaskSupplier::VDATaskSupplier() : m_bufferedFrameNumber(0), m_sizeField(0), m_pFieldFrame(NULL)
#endif
{
    m_isHaveSPS = false;
    m_isHavePPS = false;
    m_isVDAInstantiated = false;
#ifdef USE_APPLEGVA
    initializeAppleGVAMembers();
#endif 
}
    
VDATaskSupplier::~VDATaskSupplier()
{
    m_isHaveSPS = false;
    m_isHavePPS = false;
    if (true == m_isVDAInstantiated) {

        CALL_VTB_FUNC(VTDecompressionSessionInvalidate)(m_decompressionSession);
    }
    m_isVDAInstantiated = false;
    
#ifdef USE_APPLEGVA
    int returnValue;
    
    //Dispose of the main bitstream buffer pool, it was setup by pCreateVideoBitstreamPool
    if (m_pDisposeVideoBitstreamPool && m_bVideoBitstreamPoolExists) {
        returnValue = m_pDisposeVideoBitstreamPool(m_gvaInformation.createAcceleratorParameter3, 0);
        DEBUG_VTB_PRINT(printf("%s: DisposeVideoBitstreamPool returned %d\n", __FUNCTION__, returnValue);)
    }
    
    //Attempt to destroy the accelerator
    //Don't use g_mediaAcceleratorInterface after calling destroy.
    if (m_pDestroyAccelerator) {
        returnValue = m_pDestroyAccelerator(m_mediaAcceleratorInterface);
        DEBUG_VTB_PRINT(printf("%s: DestroyAccelerator returned %d\n", __FUNCTION__, returnValue);)
    }
    
    //Cleanup and exit
    releaseNALDescriptorPool();
    freeCallbackStructures(m_mainCallbackCount, m_gvaInformation.arrayPointersMainCallbackIdentifiers);
    freeCallbackStructures(m_otherCallbackCount, m_gvaInformation.arrayPointersOtherCallbackIdentifiers);
    
    initializeAppleGVAMembers();

    if (m_libHandleAppleGVA) {
        dlclose(m_libHandleAppleGVA);
    }
#endif
    
#ifdef VTB_LOAD_LIB_AT_INIT
    if (m_libHandle) {
        dlclose(m_libHandle);
    }
#endif
}
    
#ifdef USE_APPLEGVA
//My first guess at the declaration of a callback handler.
//There are three "main" callbacks: QuerySystemClockCallback, OutputQueueReadyCallback, and DecodedPictureReadyCallback
void QuerySystemClockCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
    
    DEBUG_VTB_PRINT(printf("%s has been invoked\n", __FUNCTION__);)
    return;
}

void OutputQueueReadyCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
    
    if(NULL == pApplicationContextInformation)  {
        
        DEBUG_VTB_PRINT(printf("%s has been invoked. pApplicationContextInformation is NULL\n", __FUNCTION__);)
        return;
    }
    struct _gvaInformation * pContext = (struct _gvaInformation *) pApplicationContextInformation;
    DEBUG_VTB_PRINT(printf("%s has been invoked. width = %d, height = %d. pSpecificCallbackHandlerInformation = %p\n", __FUNCTION__, pContext->width, pContext->height, pSpecificCallbackHandlerInformation);)
    return;
}
    
void DecodedPictureReadyCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
        
    if(NULL == pApplicationContextInformation)  {
        
        DEBUG_VTB_PRINT(printf("%s has been invoked. pApplicationContextInformation is NULL\n", __FUNCTION__);)
        return;
    }
    
#if 0
    struct _gvaInformation * pContext = (struct _gvaInformation *) pApplicationContextInformation;
    DEBUG_VTB_PRINT(printf("%s has been invoked. width = %d, height = %d. pSpecificCallbackHandlerInformation = %p\n", __FUNCTION__, pContext->width, pContext->height, pSpecificCallbackHandlerInformation);)
    
    if(NULL == pSpecificCallbackHandlerInformation)  {
        
        DEBUG_VTB_PRINT(printf("%s has been invoked. pSpecificCallbackHandlerInformation is NULL\n", __FUNCTION__);)
        return;
    }
#else
#if 1
    
    static unsigned int arrayIndex = 0;
    struct _decodeFrameContext frameContext;
    struct _decodeCallbackInformation * pContext = (struct _decodeCallbackInformation *) pApplicationContextInformation;
    
    TaskBrokerSingleThreadVDA * pTaskBrokerSingleThreadVDA = (TaskBrokerSingleThreadVDA *) pContext->pTaskBrokerSingleThreadVDA;
    frameContext = pContext->arrayFrameContexts[arrayIndex++];
    if(arrayIndex >= maxNumberFrameContexts)  {
        
        DEBUG_VTB_PRINT(printf(" %s Resetting pContext->arrayFrameContexts index\n", __FUNCTION__);)
        arrayIndex = 0;
    }
    
    UMC::H264DecoderFrame * pFrame = frameContext.pFrame;
    if(NULL == pFrame)
    {
        DEBUG_VTB_PRINT(printf("%s: frameContext.pFrame is NULL\n", __FUNCTION__);)
        return;
    }

//    struct _decodeFrameContext * pContext  = (struct _decodeFrameContext *) pApplicationContextInformation;
//    frameContext = pContext[arrayIndex++];
    DEBUG_VTB_PRINT(printf("%s: frame number = %d, frame type = %d\n", __FUNCTION__, frameContext.frameNumber, frameContext.frameType);)
#else
    std::deque<struct _decodeFrameContext> & pContext = (std::deque<struct _decodeFrameContext> &) pApplicationContextInformation;
        
    if (pContext.size()) {
        struct _decodeFrameContext & frameContext = pContext[0];
        DEBUG_VTB_PRINT(printf("%s: frame number = %d, frame type = %d\n", __FUNCTION__, frameContext.frameNumber, frameContext.frameType);)
        pContext.pop_front();
    }
    else    {
        DEBUG_VTB_PRINT(printf("%s: frame context deque is empty\n", __FUNCTION__);)        
    }
#endif  // 1
#endif
    
    struct AVF_PixelBufferInfo * pPixelBufferInfo = (struct AVF_PixelBufferInfo *) pSpecificCallbackHandlerInformation;
    
    DEBUG_VTB_PRINT(
                    printf("%s: Contents of AVF_PixelBufferInfo\n", __FUNCTION__);
                    printf(" offset_0_7 = %lld, pixelBuffer = %p, offset_16_23 = %lld, offset_24_31 = %lld\n", pPixelBufferInfo->offset_0_7, pPixelBufferInfo->pixelBuffer, pPixelBufferInfo->offset_16_23, pPixelBufferInfo->offset_24_31);
                    printf(" offset_24_27 = %d, offset_28_31 = %d\n", pPixelBufferInfo->offset_24_27, pPixelBufferInfo->offset_28_31);
                    )
    
    if(pPixelBufferInfo->pixelBuffer)    {
        
        CFRetain(pPixelBufferInfo->pixelBuffer);
        
        size_t width, height;
        width = CVPixelBufferGetWidth(pPixelBufferInfo->pixelBuffer);
        height = CVPixelBufferGetHeight(pPixelBufferInfo->pixelBuffer);
        DEBUG_VTB_PRINT(printf(" pixel buffer width = %d, height = %d\n", width, height);)
        
        void * pPixelBaseAddress;
        mfxU8 * pPixels, * pPixelsU, * pPixelsV;
        mfxU8 * pBaseAddressY = NULL;
        mfxU8 * pBaseAddressU = NULL;
        mfxU8 * pBaseAddressV = NULL;
        mfxU8 * pDecodedY = NULL;
        mfxU8 * pDecodedU = NULL;
        mfxU8 * pDecodedV = NULL;
        size_t bufferHeight, bufferWidth, bytesPerRow, planeCount;
        size_t bufferHeightY, bufferWidthY, bytesPerRowY;
        size_t bufferHeightU, bufferWidthU, bytesPerRowU;
        size_t bufferHeightV, bufferWidthV, bytesPerRowV;
        int row, column;
        int uvIndex, yIndex;
        
        // copy data from VDA_frame to out_y, out_u, out_v
        CVPixelBufferLockBaseAddress(pPixelBufferInfo->pixelBuffer, 0);
        planeCount = CVPixelBufferGetPlaneCount(pPixelBufferInfo->pixelBuffer);
        pPixelBaseAddress = CVPixelBufferGetBaseAddress(pPixelBufferInfo->pixelBuffer);
        
        OSType pixelFormatType = CVPixelBufferGetPixelFormatType(pPixelBufferInfo->pixelBuffer);
        if(pixelFormatType == kCVPixelFormatType_420YpCbCr8Planar)  {
            DEBUG_VTB_PRINT(printf(" pixel format = kCVPixelFormatType_420YpCbCr8Planar\n");)
        }
        else if(pixelFormatType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)  {
            
            DEBUG_VTB_PRINT(printf(" pixel format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange\n");)
            
            pBaseAddressY = (mfxU8 *) CVPixelBufferGetBaseAddressOfPlane(pPixelBufferInfo->pixelBuffer, 0);
            bytesPerRowY = CVPixelBufferGetBytesPerRowOfPlane(pPixelBufferInfo->pixelBuffer, 0);
            bufferHeightY = CVPixelBufferGetHeightOfPlane(pPixelBufferInfo->pixelBuffer, 0);
            bufferWidthY = CVPixelBufferGetWidthOfPlane(pPixelBufferInfo->pixelBuffer, 0);
            if (planeCount > 0) {
                pBaseAddressU = (mfxU8 *) CVPixelBufferGetBaseAddressOfPlane(pPixelBufferInfo->pixelBuffer, 1);
                bytesPerRowU = CVPixelBufferGetBytesPerRowOfPlane(pPixelBufferInfo->pixelBuffer, 1);
                bufferHeightU = CVPixelBufferGetHeightOfPlane(pPixelBufferInfo->pixelBuffer, 1);
                bufferWidthU = CVPixelBufferGetWidthOfPlane(pPixelBufferInfo->pixelBuffer, 1);
            }
            
            pDecodedY = pBaseAddressY;
            pDecodedU = pBaseAddressU;
            
            //Any extended pixels?  (cropping)
            size_t extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom;
            CVPixelBufferGetExtendedPixels (pPixelBufferInfo->pixelBuffer, &extraColumnsLeft, &extraColumnsRight, &extraRowsTop, &extraRowsBottom);
            if((extraColumnsLeft != 0) || (extraColumnsRight != 0) || (extraRowsTop != 0) || (extraRowsBottom != 0))    {
                DEBUG_VTB_PRINT(printf(" Extended Pixel Information: left = %d, right = %d, top = %d, bottom = %d\n", extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom);)
            }
            
            //Copy Y
            yIndex = 0;
            bufferHeight = bufferHeightY;
            bufferWidth = bufferWidthY;
            bytesPerRow = bytesPerRowY;
            pPixels = pDecodedY;
            
            mfxU8 * pDestination = pFrame->m_pYPlane;
            DEBUG_VTB_PRINT(printf(" 420YpCbCr8BiPlanarVideoRange Y: height = %d, width = %d, bytes per row = %d, output pitch = %d, plane count = %d\n", bufferHeight, bufferWidth, bytesPerRow, pFrame->pitch_luma(), planeCount);)
            IppiSize regionOfInterest;
            regionOfInterest.width = bufferWidth;
            regionOfInterest.height = bufferHeight;
            ippiCopyManaged_8u_C1R(pPixels, bytesPerRow, pDestination, pFrame->pitch_luma(), regionOfInterest, 2);
            
            /*            for(row = 0; row < bufferHeight; row++)   {
             #ifdef USE_MEMCPY
             memcpy(pDestination, pPixels, bufferWidth);
             #else
             for(column = 0, yIndex = 0; column < bufferWidth; column++, yIndex++)   {
             pDestination[yIndex] = (pPixels)[column];
             }
             #endif
             pDestination += pFrame->pitch_luma();
             pPixels += bytesPerRow;
             }*/
            
            //Copy U and V
            uvIndex = 0;
            bufferHeight = bufferHeightU;
            bufferWidth = bufferWidthU;
            bytesPerRow = bytesPerRowU;
            pPixelsU = pDecodedU;
            
            pDestination = pFrame->m_pUVPlane;
            DEBUG_VTB_PRINT(printf(" 420YpCbCr8BiPlanarVideoRange UV: height = %d, width = %d, bytes per row = %d, output pitch = %d\n", bufferHeight, bufferWidth, bytesPerRow, pFrame->pitch_chroma());)
            
            regionOfInterest.width = bufferWidth;
            regionOfInterest.height = bufferHeight;
            ippiCopyManaged_8u_C1R(pPixelsU, bytesPerRow, pDestination, pFrame->pitch_chroma(), regionOfInterest, 2);
            
            for(row = 0; row < bufferHeight; row++)   {
#ifdef USE_MEMCPY
                memcpy(pDestination, pPixelsU, 2*bufferWidth);
#else
                for(column = 0, uvIndex = 0; column < bufferWidth; column++, uvIndex += 2)   {
                    pDestination[uvIndex] = (pPixelsU)[uvIndex];
                    pDestination[uvIndex+1] = (pPixelsU)[uvIndex+1];
                }
#endif
                pPixelsU += bytesPerRow;
                pDestination += pFrame->pitch_chroma();
            }
        }
        else if(pixelFormatType == kCVPixelFormatType_422YpCbCr8)   {
            DEBUG_VTB_PRINT(printf(" pixel format = kCVPixelFormatType_422YpCbCr8\n");)
        }
        else    {
            DEBUG_VTB_PRINT(printf(" pixel format = 0x%0x\n", pixelFormatType);)
        }
        
        CVPixelBufferUnlockBaseAddress(pPixelBufferInfo->pixelBuffer, 0);
//        CVPixelBufferRelease(pPixelBufferInfo->pixelBuffer);
        CFRelease(pPixelBufferInfo->pixelBuffer);

        
        //Field or frame?
        if (pFrame->m_PictureStructureForDec < FRM_STRUCTURE)
        {
            
            //Field
            DEBUG_VTB_PRINT(printf("%s following interlaced path\n", __FUNCTION__);)
            pTaskBrokerSingleThreadVDA->AddReportItem(pFrame->m_index, 0, ERROR_FRAME_NONE);
            pTaskBrokerSingleThreadVDA->AddReportItem(pFrame->m_index, 1, ERROR_FRAME_NONE);
        }
        else
        {
            
            //Frame
            DEBUG_VTB_PRINT(printf("%s following progressive path\n", __FUNCTION__);)
            pTaskBrokerSingleThreadVDA->AddReportItem(pFrame->m_index, 0, ERROR_FRAME_NONE);
        }
    }
    
    DEBUG_VTB_PRINT(printf("%s: The first pointer is pointing to %p\n", __FUNCTION__, (void *) pPixelBufferInfo->offset_0_7);)
    
    return;
}
//End of the "main" callbacks

//There are five other callbacks: StopCallback, FlushCallback, BufferRequirementCallback, VideoDecoderSequenceInfoCallback, and ContextCreationInfoCallback
void StopCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
    
    DEBUG_VTB_PRINT(printf("%s has been invoked\n", __FUNCTION__);)
    struct _gvaInformation * pContext = (struct _gvaInformation *) pApplicationContextInformation;
    return;
}

void FlushCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
    
    DEBUG_VTB_PRINT(printf("%s has been invoked\n", __FUNCTION__);)
    struct _gvaInformation * pContext = (struct _gvaInformation *) pApplicationContextInformation;
    return;
}

void BufferRequirementCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
    
    DEBUG_VTB_PRINT(printf("%s has been invoked\n", __FUNCTION__);)
    struct _gvaInformation * pContext = (struct _gvaInformation *) pApplicationContextInformation;
    return;
}

void VideoDecoderSequenceInfoCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
    
    if(NULL == pApplicationContextInformation)  {
        
        DEBUG_VTB_PRINT(printf("%s has been invoked. pApplicationContextInformation is NULL\n", __FUNCTION__);)
        return;
    }
    struct _gvaInformation * pContext = (struct _gvaInformation *) pApplicationContextInformation;
    DEBUG_VTB_PRINT(printf("%s has been invoked. width = %d, height = %d. pSpecificCallbackHandlerInformation = %p\n", __FUNCTION__, pContext->width, pContext->height, pSpecificCallbackHandlerInformation);)
    return;
}

void ContextCreationInfoCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation)  {
    
    DEBUG_VTB_PRINT(printf("%s Entry\n", __FUNCTION__);)
    if(NULL == pApplicationContextInformation)  {
        
        DEBUG_VTB_PRINT(printf(" pApplicationContextInformation is NULL\n");)
        return;
    }
    else    {
        struct _gvaInformation * pContext = (struct _gvaInformation *) pApplicationContextInformation;
        DEBUG_VTB_PRINT(printf(" width = %d, height = %d\n", pContext->width, pContext->height);)        
    }
    if(NULL == pSpecificCallbackHandlerInformation)  {
        
        DEBUG_VTB_PRINT(printf(" pSpecificCallbackHandlerInformation is NULL\n");)
        return;
    }
    else    {
        uint32_t * pContextCreationInfoValue = (uint32_t *) pSpecificCallbackHandlerInformation;
        DEBUG_VTB_PRINT(printf(" ContextCreationInfoValue = %d\n", *pContextCreationInfoValue);)
    }
    return;
}
//End of the other callbacks
    
//Name: loadAppleGVALibrary
//Description: Load the AppleGVA library, and resolve symbols.
Status VDATaskSupplier::loadAppleGVALibrary()
{
    
    //use dlopen to load the library
    m_libHandleAppleGVA = dlopen("/System/Library/PrivateFrameworks/AppleGVA.framework/Versions/Current/AppleGVA", RTLD_LOCAL | RTLD_NOW);
    if (!m_libHandleAppleGVA) {
        DEBUG_VTB_PRINT(printf("%s Unable to open AppleGVA library: %s\n",  __FUNCTION__, dlerror());)
        return UMC_ERR_FAILED;
    }
    
    //Resolve the AVF_CreateMediaAcceleratorInterface symbol
    void * (* p_AVF_CreateMediaAcceleratorInterface) (void *, int32_t);
    p_AVF_CreateMediaAcceleratorInterface = (void * (*) (void *, int32_t)) dlsym(m_libHandleAppleGVA, "AVF_CreateMediaAcceleratorInterface");
    if (!p_AVF_CreateMediaAcceleratorInterface) {
        DEBUG_VTB_PRINT(printf("%s Unable to resolve symbol for AVF_CreateMediaAcceleratorInterface: %s\n",  __FUNCTION__, dlerror());)
        return UMC_ERR_FAILED;
    }
    
    //Create the media accelerator interface, and array of pointers.
    //The memory for the array of pointers is allocated by p_AVF_CreateMediaAcceleratorInterface,
    //and will be freed by DestroyAccelerator.
    int32_t createParameter_2 = 2;
    p_AVF_CreateMediaAcceleratorInterface(&m_mediaAcceleratorInterface, createParameter_2);
    DEBUG_VTB_PRINT(printf("%s Created AVF_CreateMediaAcceleratorInterface\n", __FUNCTION__);)
    
    return UMC_OK;
}
    
void VDATaskSupplier::freeCallbackStructures(unsigned int callBackIdentifierCount, AVF_Callback * * arrayPointersCallbackIdentifiers)
{
    if(arrayPointersCallbackIdentifiers)
    {
        for(unsigned int index = 0; index < callBackIdentifierCount; index++)
        {
            if(arrayPointersCallbackIdentifiers[index])
            {
                if(arrayPointersCallbackIdentifiers[index]->pCallbackInformation)
                {
                    free(arrayPointersCallbackIdentifiers[index]->pCallbackInformation);
                }
                free(arrayPointersCallbackIdentifiers[index]);
            }
        }
        
        free(arrayPointersCallbackIdentifiers);
    }
}
    
struct _functionStructure * * VDATaskSupplier::prepareFunctions(uint32_t parameter_1, uint32_t * pOutput, uint32_t parameter_3, uint32_t parameter_4, uint32_t parameter_5, uint32_t parameter_6)
{
    
    //QT's prepareFunctions calculates the following by this
    uint32_t outputValue = 0;
    outputValue = parameter_5 | parameter_6;
    if (outputValue != 0) {
        outputValue = 1;
    }
    //    if(parameter_3 > 1)
    
    outputValue = 1;
    pOutput[0] = outputValue;
    
    //QT'S prepareFunctions also does this.
    //Note: It can do more than this. I copied exactly what happens in the QT execution.
    struct _functionStructure * * * pStructures;
    
    if(NULL == (pStructures = (struct _functionStructure * * *) malloc(sizeof(struct _functionStructure * *))))
    {
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for pointer to pointers to function structures\n", __FUNCTION__);)
        return NULL;
    }
    
    struct _functionStructure * * prepareStructure;
    
    if(NULL == (prepareStructure = (struct _functionStructure * *) malloc(sizeof(struct _functionStructure *))))
    {
        DEBUG_VTB_PRINT(printf("%s : Error, could not allocate memory for pointers to function structures\n", __FUNCTION__);)
        free(pStructures);
        return NULL;
    }
    
    if(NULL == (*prepareStructure = (struct _functionStructure *) malloc(sizeof(struct _functionStructure))))
    {
        DEBUG_VTB_PRINT(printf("%s : Error, could not allocate memory for function structures\n", __FUNCTION__);)
        free(pStructures);
        return NULL;
    }

    *pStructures = prepareStructure;
    (*(prepareStructure))->id = 1;
    (*(prepareStructure))->reserved = 0;
    (*(prepareStructure))->prepareFunctions_parameter_4 = parameter_4;
    (*(prepareStructure))->format_1_and_2[0] = 'b';
    (*(prepareStructure))->format_1_and_2[1] = 'w';
    (*(prepareStructure))->format_1_and_2[2] = 'a';
    (*(prepareStructure))->format_1_and_2[3] = 'r';
    (*(prepareStructure))->format_1_and_2[4] = '2';
    (*(prepareStructure))->format_1_and_2[5] = '1';
    (*(prepareStructure))->format_1_and_2[6] = 'V';
    (*(prepareStructure))->format_1_and_2[7] = 'N';
    
    return *pStructures;
}


AVF_Callback * * VDATaskSupplier::prepareMainCallbacks(unsigned int callBackIdentifierCount, struct _gvaInformation * p_testapp_info)
{
    callbackInformation * pcallbackInformation_QuerySystemClock = (callbackInformation *) malloc(sizeof(callbackInformation));
    
    if(NULL == pcallbackInformation_QuerySystemClock)
    {
        
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the QuerySystemClock callback information structure\n", __FUNCTION__);)
        return NULL;
    }
    
    callbackInformation * pcallbackInformation_OutputQueueReady = (callbackInformation *) malloc(sizeof(callbackInformation));
    
    if(NULL == pcallbackInformation_OutputQueueReady)
    {
        
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the OutputQueueReady callback information structure\n", __FUNCTION__);)
        free(pcallbackInformation_QuerySystemClock);
        return NULL;
    }
    callbackInformation * pcallbackInformation_DecodedPictureReady = (callbackInformation *) malloc(sizeof(callbackInformation));
    if(NULL == pcallbackInformation_DecodedPictureReady)
    {
        
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the DecodedPictureReady callback information structure\n", __FUNCTION__);)
        free(pcallbackInformation_QuerySystemClock);
        free(pcallbackInformation_OutputQueueReady);
        return NULL;
    }
    
    AVF_Callback * * pArrayPointers = (AVF_Callback * *) malloc(callBackIdentifierCount*sizeof(AVF_Callback *));
    if(NULL == pArrayPointers)
    {
        
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the array of pointers to main callback identifier structures\n", __FUNCTION__);)
        free(pcallbackInformation_QuerySystemClock);
        free(pcallbackInformation_OutputQueueReady);
        free(pcallbackInformation_DecodedPictureReady);
        return NULL;
    }
    
    for(unsigned int index = 0; index < callBackIdentifierCount; index++)
    {
        AVF_Callback * pcallbackIdentifier = (AVF_Callback *) malloc(sizeof(AVF_Callback));
        pArrayPointers[index] = pcallbackIdentifier;
        if(NULL == pcallbackIdentifier) {
            DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for callback identifier structure %d\n", __FUNCTION__, index);)
        }
    }
    
    //Setup the connections for QuerySystemClockCallback
    pcallbackInformation_QuerySystemClock->pContextInformation = (void *) p_testapp_info;
    pcallbackInformation_QuerySystemClock->callbackFunction = QuerySystemClockCallback;
    pArrayPointers[0]->idNumber = APPLEGVA_CALLBACK_MAIN_QUERY_SYSTEMCLOCK;    //because qt's prepareMainCallbacks sets this value for QuerySystemClock
    pArrayPointers[0]->reserved = 0;
    pArrayPointers[0]->pCallbackInformation = pcallbackInformation_QuerySystemClock;
    
    //Setup the connections for OutputQueueReadyCallback
    pcallbackInformation_OutputQueueReady->pContextInformation = (void *) p_testapp_info;
    pcallbackInformation_OutputQueueReady->callbackFunction = OutputQueueReadyCallback;
    pArrayPointers[1]->idNumber = APPLEGVA_CALLBACK_MAIN_OUTPUT_QUEUE_READY;    //because qt's prepareMainCallbacks sets this value for OutputQueueReady
    pArrayPointers[1]->reserved = 0;
    pArrayPointers[1]->pCallbackInformation = pcallbackInformation_OutputQueueReady;
    
    //Setup the connections for DecodedPictureReadyCallback
//    pcallbackInformation_DecodedPictureReady->pContextInformation = (void *) m_arrayCallbackContext;//&m_callbackContext;//(void *) p_testapp_info;
    pcallbackInformation_DecodedPictureReady->pContextInformation = (void *) &m_callbackContextInformation;
    pcallbackInformation_DecodedPictureReady->callbackFunction = DecodedPictureReadyCallback;
    pArrayPointers[2]->idNumber = APPLEGVA_CALLBACK_MAIN_DECODED_PICTURE_READY;    //because qt's prepareMainCallbacks sets this value for DecodedPictureReady
    pArrayPointers[2]->reserved = 0;
    pArrayPointers[2]->pCallbackInformation = pcallbackInformation_DecodedPictureReady;
    
    return pArrayPointers;
}

AVF_Callback * * VDATaskSupplier::prepareCallbacks(unsigned int callBackIdentifierCount, struct _gvaInformation * p_testapp_info)
{
    callbackInformation * pcallbackInformation_Stop = (callbackInformation *) malloc(sizeof(callbackInformation));
    if(NULL == pcallbackInformation_Stop)
    {
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the Stop callback information structure\n", __FUNCTION__);)
        return NULL;
    }
    callbackInformation * pcallbackInformation_Flush = (callbackInformation *) malloc(sizeof(callbackInformation));
    if(NULL == pcallbackInformation_Flush)
    {
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the Flush callback information structure\n", __FUNCTION__);)
        free(pcallbackInformation_Stop);
        return NULL;
    }
    callbackInformation * pcallbackInformation_BufferRequirement = (callbackInformation *) malloc(sizeof(callbackInformation));
    if(NULL == pcallbackInformation_BufferRequirement)
    {
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the BufferRequirement callback information structure\n", __FUNCTION__);)
        free(pcallbackInformation_Stop);
        free(pcallbackInformation_Flush);
        return NULL;
    }
    callbackInformation * pcallbackInformation_VideoDecoderSequenceInfo = (callbackInformation *) malloc(sizeof(callbackInformation));
    if(NULL == pcallbackInformation_VideoDecoderSequenceInfo)
    {
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the VideoDecoderSequenceInfo callback information structure\n", __FUNCTION__);)
        free(pcallbackInformation_Stop);
        free(pcallbackInformation_Flush);
        free(pcallbackInformation_BufferRequirement);
        return NULL;
    }
    callbackInformation * pcallbackInformation_ContextCreationInfo = (callbackInformation *) malloc(sizeof(callbackInformation));
    if(NULL == pcallbackInformation_ContextCreationInfo)
    {
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the ContextCreationInfo callback information structure\n", __FUNCTION__);)
        free(pcallbackInformation_Stop);
        free(pcallbackInformation_Flush);
        free(pcallbackInformation_BufferRequirement);
        free(pcallbackInformation_VideoDecoderSequenceInfo);
        return NULL;
    }
    
    AVF_Callback * * pArrayPointers = (AVF_Callback * *) malloc(callBackIdentifierCount*sizeof(AVF_Callback *));
    if(NULL == pArrayPointers)
    {
        
        DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for the array of pointers to callback identifier structures\n", __FUNCTION__);)
        free(pcallbackInformation_Stop);
        free(pcallbackInformation_Flush);
        free(pcallbackInformation_BufferRequirement);
        free(pcallbackInformation_VideoDecoderSequenceInfo);
        free(pcallbackInformation_ContextCreationInfo);
        return NULL;
    }
    
    for(unsigned int index = 0; index < callBackIdentifierCount; index++)
    {
        AVF_Callback * pcallbackIdentifier = (AVF_Callback *) malloc(sizeof(AVF_Callback));
        pArrayPointers[index] = pcallbackIdentifier;
        if(NULL == pcallbackIdentifier) {
            DEBUG_VTB_PRINT(printf("%s: Error, could not allocate memory for callback identifier structure %d\n", __FUNCTION__, index);)
        }
    }
    
    //Setup the connections for StopCallback
    pcallbackInformation_Stop->pContextInformation = (void *) p_testapp_info;
    pcallbackInformation_Stop->callbackFunction = StopCallback;
    pArrayPointers[0]->idNumber = APPLEGVA_CALLBACK_STOP;                   //because qt's prepareCallbacks sets this value for StopCallback
    pArrayPointers[0]->reserved = 0;
    pArrayPointers[0]->pCallbackInformation = pcallbackInformation_Stop;
    
    //Setup the connections for FlushCallback
    pcallbackInformation_Flush->pContextInformation = (void *) NULL;
    pcallbackInformation_Flush->callbackFunction = FlushCallback;
    pArrayPointers[1]->idNumber = APPLEGVA_CALLBACK_FLUSH;                  //because qt's prepareCallbacks sets this value for FlushCallback
    pArrayPointers[1]->reserved = 0;
    pArrayPointers[1]->pCallbackInformation = pcallbackInformation_Flush;
    
    //Setup the connections for BufferRequirementCallback
    pcallbackInformation_BufferRequirement->pContextInformation = (void *) NULL;
    pcallbackInformation_BufferRequirement->callbackFunction = BufferRequirementCallback;
    pArrayPointers[2]->idNumber = APPLEGVA_CALLBACK_BUFFER_REQUIREMENT;    //because qt's prepareCallbacks sets this value for BufferRequirementCallback
    pArrayPointers[2]->reserved = 0;
    pArrayPointers[2]->pCallbackInformation = pcallbackInformation_BufferRequirement;
    
    //Setup the connections for VideoDecoderSequenceInfoCallback
    pcallbackInformation_VideoDecoderSequenceInfo->pContextInformation = (void *) p_testapp_info;
    pcallbackInformation_VideoDecoderSequenceInfo->callbackFunction = VideoDecoderSequenceInfoCallback;
    pArrayPointers[3]->idNumber = APPLEGVA_CALLBACK_VIDEO_DECODER_SEQUENCE;    //because qt's prepareCallbacks sets this value for VideoDecoderSequenceInfoCallback
    pArrayPointers[3]->reserved = 0;
    pArrayPointers[3]->pCallbackInformation = pcallbackInformation_VideoDecoderSequenceInfo;
    
    //Setup the connections for ContextCreationInfoCallback
    pcallbackInformation_ContextCreationInfo->pContextInformation = (void *) p_testapp_info;
    pcallbackInformation_ContextCreationInfo->callbackFunction = ContextCreationInfoCallback;
    pArrayPointers[4]->idNumber = APPLEGVA_CALLBACK_CONTEXT_CREATION;    //because qt's prepareCallbacks sets this value for ContextCreationInfoCallback
    pArrayPointers[4]->reserved = 0;
    pArrayPointers[4]->pCallbackInformation = pcallbackInformation_ContextCreationInfo;
    
    return pArrayPointers;
}
#endif //USE_APPLEGVA
    
void VDATaskSupplier::AddToNALQueue(struct nalStreamInfo nalInfo)
{
    
    const int initialLEPValue = -1;
    static int lastEntryPushed = initialLEPValue;

    //See if dummy nalInfo entries must be pushed.
    //The dummy entries represent IDR or non_IDR NALs.
    if((lastEntryPushed != initialLEPValue) && (nalInfo.nalNumber > lastEntryPushed + 1))  {
        
        unsigned int dummyCount = nalInfo.nalNumber - (lastEntryPushed + 1);
        struct nalStreamInfo dummyNALInfo;
        DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s pushing %d dummy entries onto prepend data queue\n", __FUNCTION__, dummyCount);)
        
        //push dummy entries equal to the number of non-header NALs
        //between the non-header NAL being processed now,
        //and the last time a non-header NAL was processed.
        for (unsigned count = 0; count < dummyCount; count++) {
            dummyNALInfo.nalNumber = lastEntryPushed + 1 + count;
            dummyNALInfo.nalType = NAL_UT_SLICE;    //Just has to be different than SPS, PPS, SEI
            m_prependDataQueue.push(dummyNALInfo);
        }
    }
    m_prependDataQueue.push(nalInfo);
    lastEntryPushed = nalInfo.nalNumber;
    
}
    
Status VDATaskSupplier::DecodeSEI(MediaDataEx *nalUnit)
{
    DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s Entry\n", __FUNCTION__);)
    
    Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
    Ipp32u nalNumber = nalUnit->GetExData()->nalNumber;
    
    Ipp8u * pSei = (Ipp8u*) nalUnit->GetDataPointer();
    unsigned int seiLength = nalUnit->GetDataSize();
    
    try
    {
        
        struct nalStreamInfo nalInfo;
        nalInfo.nalNumber = nalNumber;
        nalInfo.nalType = nal_unit_type;
        
        //Need length of SEI
        Ipp32u bigEndianSEILength32 = htonl(seiLength);
        nalInfo.headerBytes.push_back((Ipp8u) (bigEndianSEILength32 & 0x00ff));         //LSB of SPS Length (expressed in network order)
        nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSEILength32 >> 8) & 0x00ff));  //MSB of SPS Length (expressed in network order)
        nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSEILength32 >> 16) & 0x00ff));  //MSB of SPS Length (expressed in network order)
        nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSEILength32 >> 24) & 0x00ff));  //MSB of SPS Length (expressed in network order)

        for(unsigned int index = 0; index < seiLength; index++)    {
            
            nalInfo.headerBytes.push_back(pSei[index]);
        }

#ifdef RMH_USE_ADDTONALQUEUE
        m_nalQueueMutex.Lock();
        AddToNALQueue(nalInfo);
        m_nalQueueMutex.Unlock();
#endif
        
        
    }   catch(...)
    {
        // nothing to do just catch it
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s caught exception\n", __FUNCTION__);)
    }

    return MFXTaskSupplier::DecodeSEI(nalUnit);
}

Status VDATaskSupplier::Init(BaseCodecParams *pInit)
{
    
#ifdef USE_APPLEGVA
    Status gvaResult;
    
    gvaResult = loadAppleGVALibrary();
    if(gvaResult != UMC_OK) {
        
        //Could not load the AppleGVA library
        return gvaResult;
    }
    
    //Try to create the accelerator
    //Instantiate and initialize my version of qt test app's testapp_info
    bzero(&m_gvaInformation, sizeof(struct _gvaInformation)); //CreateAccelerator will fail if the structure is not initialized to zeros!
    
    m_gvaInformation.createAcceleratorParameter3 = NULL;
    m_gvaInformation.quartzDisplayID = CGMainDisplayID();
    DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s Display ID = %d (0x%x)\n", __FUNCTION__, m_gvaInformation.quartzDisplayID, m_gvaInformation.quartzDisplayID );)
    m_gvaInformation.valueAfterDisplayID = 1;
    
    //Setup the three function callbacks that qt test app used.
    m_gvaInformation.numberOfMainCallbacks = m_mainCallbackCount;
    m_gvaInformation.arrayPointersMainCallbackIdentifiers = prepareMainCallbacks(m_mainCallbackCount, &m_gvaInformation);
    m_gvaInformation.arrayPointersOtherCallbackIdentifiers = prepareCallbacks(m_otherCallbackCount, &m_gvaInformation);
    
    //Attempt to create the accelerator
    int gvaReturnValue;
    
    m_pCreateAccelerator = (int (* ) (int32_t, uint32_t *, void * * *)) m_mediaAcceleratorInterface[2];
    
    gvaReturnValue = m_pCreateAccelerator(2, &m_gvaInformation.quartzDisplayID, &m_gvaInformation.createAcceleratorParameter3);
    DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s CreateAccelerator returned %d, parameter 3 value = %p\n", __FUNCTION__, gvaReturnValue, m_gvaInformation.createAcceleratorParameter3);)
    
    if (gvaReturnValue == APPLEGVA_RETURN_PASS) {
        
        //Initialize other function pointers
        m_pDisposeVideoBitstreamPool = (int (*) (void *, uint32_t)) m_mediaAcceleratorInterface[8];
        m_pDestroyAccelerator = (int (*) (void *)) m_mediaAcceleratorInterface[26];
        m_pSetDecodeDescription = (int (*) (void *, uint32_t, uint32_t, uint32_t, void *)) m_mediaAcceleratorInterface[22];
        m_pCreateMainVideoDataPath = (int (*) (void *, uint32_t, struct _prepareStructure *)) m_mediaAcceleratorInterface[4];
        m_pCreateVideoBitstreamPool = (int (*) (void *, uint32_t, uint32_t *)) m_mediaAcceleratorInterface[7];
        m_pSetScanMode = (int (*) (void *, uint64_t, uint32_t *)) m_mediaAcceleratorInterface[24];
        m_pQueryRenderMode = (int (*) (void *, uint64_t, uint32_t *)) m_mediaAcceleratorInterface[25];
        
        m_pGetNALBufferAddress = (int (*) (void *, uint32_t, uint32_t, unsigned char * *)) m_mediaAcceleratorInterface[10];         //AppleGVA Function 8
        m_pSetNALBufferReady = (int (*) (void *, uint32_t, uint32_t)) m_mediaAcceleratorInterface[15];                              //AppleGVA Function 13
        m_pSetNALDescriptors = (int (*) (void *, uint32_t, uint32_t, uint32_t, NALDescriptor * *)) m_mediaAcceleratorInterface[16]; //AppleGVA Function 14
        m_pFunction15 = (int (*) (void *, uint32_t, uint32_t, NALDescriptor * *)) m_mediaAcceleratorInterface[17];                  //AppleGVA Function 15
        m_pGetDecoderStatistics = (int (*) (void *, uint32_t, uint32_t, uint32_t, uint32_t *)) m_mediaAcceleratorInterface[23];    //AppleGVA Function 21
        
        //Get the render mode
        //This will clear the value of the third parameter if quartzDisplayID is not equal to -1.
        //It will set the value of the third parametr if quartzDisplayID is equal to -1.
        gvaReturnValue = m_pQueryRenderMode(m_gvaInformation.createAcceleratorParameter3, 6, &m_gvaInformation.queryRenderModeParameter3);
        DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s QueryRenderMode returned %d, parameter 3 value = %d\n", __FUNCTION__, gvaReturnValue, m_gvaInformation.queryRenderModeParameter3);)
        
        //Create a pool of NALDescriptors
        if(NULL == (m_NALDescriptorPool = (NALDescriptor * *) malloc(m_sizeNALDescriptorPool*sizeof(NALDescriptor *))))   {
            
            DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s Unable to malloc memory for array of pointers to NALDescriptors\n", __FUNCTION__);)
            return UMC_ERR_INIT;            
        }
        for (unsigned int i = 0; i < m_sizeNALDescriptorPool; i++) {
            if(NULL == (m_NALDescriptorPool[i] = (NALDescriptor *) malloc(sizeof(NALDescriptor))))   {
                DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s Unable to malloc memory for NALDescriptor %d\n", __FUNCTION__, i);)
                for (unsigned int j = 0; j < i; j++) {
                    
                    free(m_NALDescriptorPool[j]);
                }
                free(m_NALDescriptorPool);
            }
            m_NALDescriptorPool[i]->sizeNAL = 0;
            m_NALDescriptorPool[i]->reserved = 0;
            m_NALDescriptorPool[i]->pNALData = NULL;
        }
        
    }
    else    {
        
        //Couldn't create the accelerator
        DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s Could not create AppleGVA accelerator\n", __FUNCTION__));
        return(UMC_ERR_FAILED);
    }

    
#endif //USE_APPLEGVA
    
#ifdef VTB_LOAD_LIB_AT_INIT
    if(m_libHandle == NULL)
    {
        
        //Open the VideoToolbox library.
        //Try the public framework first, if that fails to load then try private framework.
        m_libHandle = dlopen("/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox", RTLD_LOCAL | RTLD_NOW);
        if (!m_libHandle) {
            DEBUG_PRINT("VDATaskSupplier::%s Attempting to load from private frameworks\n", __FUNCTION__);
            m_libHandle = dlopen("/System/Library/PrivateFrameworks/VideoToolbox.framework/VideoToolbox", RTLD_LOCAL | RTLD_NOW);
            if (!m_libHandle) {
                DEBUG_PRINT("VDATaskSupplier::%s Unable to open library: %s\n",  __FUNCTION__, dlerror());
                return UMC_ERR_INIT;
            }
        }
        
        //Get VideoToolbox symbols
        call_VTDecompressionSessionCreate = (OSStatus
                                             (*)
                                             (CFAllocatorRef,
                                              CMVideoFormatDescriptionRef,
                                              CFDictionaryRef,
                                              CFDictionaryRef,
                                              const VTDecompressionOutputCallbackRecord *,
                                              VTDecompressionSessionRef *))
                                             dlsym(m_libHandle, "VTDecompressionSessionCreate");
        
        if (!call_VTDecompressionSessionCreate) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionCreate: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
        
        call_VTDecompressionSessionInvalidate = (void (* ) (VTDecompressionSessionRef)) dlsym(m_libHandle, "VTDecompressionSessionInvalidate");
        if (!call_VTDecompressionSessionInvalidate) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionInvalidate: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
        
        call_VTDecompressionSessionGetTypeID = (CFTypeID (*) (void)) dlsym(m_libHandle, "VTDecompressionSessionGetTypeID");
        if (!call_VTDecompressionSessionGetTypeID) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionGetTypeID: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
        
        call_VTDecompressionSessionDecodeFrame = (OSStatus (*) (VTDecompressionSessionRef,
                                                             CMSampleBufferRef,
                                                             VTDecodeFrameFlags,
                                                             void *,
                                                             VTDecodeInfoFlags *))
                                                            dlsym(m_libHandle, "VTDecompressionSessionDecodeFrame");
        if (!call_VTDecompressionSessionDecodeFrame) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionDecodeFrame: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
        
        call_VTDecompressionSessionFinishDelayedFrames = (OSStatus (*) (VTDecompressionSessionRef)) dlsym(m_libHandle, "VTDecompressionSessionFinishDelayedFrames");
        if (!call_VTDecompressionSessionFinishDelayedFrames) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionFinishDelayedFrames: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
        
        call_VTDecompressionSessionCanAcceptFormatDescription = (Boolean (*) (VTDecompressionSessionRef, CMFormatDescriptionRef)) dlsym(m_libHandle, "VTDecompressionSessionCanAcceptFormatDescription");
        if (!call_VTDecompressionSessionCanAcceptFormatDescription) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionCanAcceptFormatDescription: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
        
        call_VTDecompressionSessionWaitForAsynchronousFrames = (OSStatus (*) (VTDecompressionSessionRef)) dlsym(m_libHandle, "VTDecompressionSessionWaitForAsynchronousFrames");
        if (!call_VTDecompressionSessionWaitForAsynchronousFrames) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionWaitForAsynchronousFrames: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
        
        call_VTDecompressionSessionCopyBlackPixelBuffer = (OSStatus (*) (VTDecompressionSessionRef, CVPixelBufferRef)) dlsym(m_libHandle, "VTDecompressionSessionCopyBlackPixelBuffer");
        if (!call_VTDecompressionSessionCopyBlackPixelBuffer) {
            DEBUG_PRINT("VDATaskSupplier::%s Unable to resolve VTDecompressionSessionCopyBlackPixelBuffer: %s\n",  __FUNCTION__, dlerror());
            return UMC_ERR_INIT;
        }
    }
#endif  //VTB_LOAD_LIB_AT_INIT
    
    m_pMemoryAllocator = pInit->lpMemoryAllocator;

    Status umsRes = TaskSupplier::Init(pInit);
    if (umsRes != UMC_OK)
    {
        DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s TaskSupplier::Init returned %d\n", __FUNCTION__, umsRes));
        return umsRes;
    }

    Ipp32u i;
    for (i = 0; i < m_iThreadNum; i += 1)
    {
        delete m_pSegmentDecoder[i];
        m_pSegmentDecoder[i] = 0;
    }

    //if (m_va && m_va->m_Profile != H264_VLD && m_va->m_Profile != H264_VLD_L)
    m_iThreadNum = 1;
    delete m_pTaskBroker;
    m_pTaskBroker = new TaskBrokerSingleThreadVDA(this);
    
#ifdef USE_APPLEGVA
    m_callbackContextInformation.pTaskBrokerSingleThreadVDA = m_pTaskBroker;
    for(unsigned int i = 0; i < maxNumberFrameContexts; i++)
    {
        m_callbackContextInformation.arrayFrameContexts[i].frameNumber = 0;
        m_callbackContextInformation.arrayFrameContexts[i].frameType = 0;
        m_callbackContextInformation.arrayFrameContexts[i].pFrame = NULL;
    }
#endif
    
    m_pTaskBroker->Init(m_iThreadNum, true);

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264_VDA_SegmentDecoder(this);
        if (0 == m_pSegmentDecoder[i])
        {
            DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s Could not allocate m_pSegmentDecoder[%d]\n", __FUNCTION__, i));
            return UMC_ERR_ALLOC;
        }
    }

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC_OK != m_pSegmentDecoder[i]->Init(i))
        {
            DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s Could not init m_pSegmentDecoder[%d]\n", __FUNCTION__, i));
            return UMC_ERR_INIT;
        }
    }

    m_DPBSizeEx = 1;

    m_sei_messages = new SEI_Storer();
    m_sei_messages->Init();
    
    m_sizeField = 0;
    m_pFieldFrame = NULL;
    while (!m_prependDataQueue.empty())
    {
        m_prependDataQueue.pop();
    }

    return UMC_OK;
}
    
void VDATaskSupplier::Close()
{
    TaskSupplier::Close();
    
    //Destroy the VTB decoder session if instantiated.
    if(m_isVDAInstantiated)     {

        CALL_VTB_FUNC(VTDecompressionSessionInvalidate)(m_decompressionSession);
        m_isVDAInstantiated = false;
    }
    
    m_sizeField = 0;
    m_pFieldFrame = NULL;
}

H264DecoderFrame *VDATaskSupplier::GetFreeFrame(const H264Slice * pSlice)
{
    AutomaticUMCMutex guard(m_mGuard);
    ViewItem &view = *GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
    H264DecoderFrame *pFrame = 0;
    
    H264DBPList *pDPB = view.GetDPBList(0);
    
    // Traverse list for next disposable frame
    pFrame = pDPB->GetDisposable();

    VM_ASSERT(!pFrame || pFrame->GetBusyState() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (pDPB->countAllFrames() >= view.dpbSize + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H264DecoderFrameExtension(m_pMemoryAllocator, &m_Heap, &m_ObjHeap);
        if (NULL == pFrame)
        {
            return 0;
        }

        pDPB->append(pFrame);

        pFrame->m_index = -1;
    }

    DecReferencePictureMarking::Remove(pFrame);
    pFrame->Reset();

    // Set current as not displayable (yet) and not outputted. Will be
    // updated to displayable after successful decode.
    pFrame->unsetWasOutputted();
    pFrame->unSetisDisplayable();
    pFrame->SetSkipped(false);
    pFrame->SetFrameExistFlag(true);
    pFrame->IncrementReference();

    if (GetAuxiliaryFrame(pFrame))
    {
        GetAuxiliaryFrame(pFrame)->Reset();
    }

    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;

    return pFrame;
}
    
void VDATaskSupplier::SetBufferedFramesNumber(Ipp32u buffered)
{
    m_DPBSizeEx = 1 + buffered;
    m_bufferedFrameNumber = buffered;
    
    if (buffered)
        DPBOutput::Reset(true);
}

Status VDATaskSupplier::RunDecoding(bool force, H264DecoderFrame ** decoded)
{
    return MFXTaskSupplier::RunDecoding(force, decoded);
}
    
Status VDATaskSupplier::AddSource(MediaData * pSource, MediaData *dst)
{
    
    Ipp32s iCode = m_pNALSplitter->CheckNalUnitType(pSource);
    
    return MFXTaskSupplier::AddSource(pSource, dst);
}

Status VDATaskSupplier::DecodeHeaders(MediaDataEx *nalUnit)
{
    
    static unsigned int counter = 0;
#ifndef RMH_USE_ADDTONALQUEUE
    const int initialLEPValue = -1;
    static int lastEntryPushed = initialLEPValue;
#endif
    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Entry, counter = %d\n", __FUNCTION__, ++counter);)
    
    Status sts = TaskSupplier::DecodeHeaders(nalUnit);
    if (sts != UMC_OK)
        return sts;
   
    H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();
    
    if (currSPS)
    {
        if (currSPS->bit_depth_luma > 8 || currSPS->bit_depth_chroma > 8 || currSPS->chroma_format_idc > 1)
            throw h264_exception(UMC_ERR_UNSUPPORTED);

        switch (currSPS->profile_idc)
        {
        case H264VideoDecoderParams::H264_PROFILE_UNKNOWN:
        case H264VideoDecoderParams::H264_PROFILE_BASELINE:
        case H264VideoDecoderParams::H264_PROFILE_MAIN:
        case H264VideoDecoderParams::H264_PROFILE_EXTENDED:
        case H264VideoDecoderParams::H264_PROFILE_HIGH:

        case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
        case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:

        case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
        case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
            break;
        default:
            throw h264_exception(UMC_ERR_UNSUPPORTED);
        }

        DPBOutput::OnNewSps(currSPS);
    }
    
    // save sps/pps
    static Ipp8u start_code_prefix[] = {0, 0, 0, 1};
    const int lengthPreamble = 4;
    RawHeader * hdr;
    size_t size;
    Ipp32s id;
    Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
    Ipp32u nalNumber = nalUnit->GetExData()->nalNumber;
    
#ifdef USE_APPLEGVA
    unsigned char *pRawSPS;
#endif
    switch(nal_unit_type)
    {
        case NAL_UT_SPS:
        {
            size = nalUnit->GetDataSize();
            DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s SPS size = %d, nal number = %d\n", __FUNCTION__, size, nalNumber);)
            hdr = GetSPS();
            id = m_Headers.m_SeqParams.GetCurrentID();
            hdr->Resize(id, size + sizeof(start_code_prefix));
            memcpy(hdr->GetPointer(), start_code_prefix,  sizeof(start_code_prefix));
            memcpy(hdr->GetPointer() + sizeof(start_code_prefix), (Ipp8u*)nalUnit->GetDataPointer(), size);                   
            hdr->SetRBSPSize(size);                   
            m_isHaveSPS = true;
            if(true == m_isVDAInstantiated)    {
                
                //VDA decoder is already instantiated, is this a new SPS-PPS sequence?
                RawHeader * pSPSHeader = GetSPS();
                int spsLength = pSPSHeader->GetRBSPSize();
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s VTB decoder is instantiated, SPS is in bitstream, length = %d\n", __FUNCTION__, spsLength));
                
                struct nalStreamInfo nalInfo;
                nalInfo.nalNumber = nalNumber;
                nalInfo.nalType = nal_unit_type;
                
                //Need length of SPS 
                Ipp32u bigEndianSPSLength32 = htonl(spsLength);
                nalInfo.headerBytes.push_back((Ipp8u) (bigEndianSPSLength32 & 0x00ff));         //LSB of SPS Length (expressed in network order)
                nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSPSLength32 >> 8) & 0x00ff));  //MSB of SPS Length (expressed in network order)
                nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSPSLength32 >> 16) & 0x00ff));  //MSB of SPS Length (expressed in network order)
                nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSPSLength32 >> 24) & 0x00ff));  //MSB of SPS Length (expressed in network order)
                
                //Copy raw bytes of SPS to this local nalStreamInfo
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Data for SPS in bitstream:", __FUNCTION__));
                Ipp8u * pRawSPSBytes = pSPSHeader->GetPointer()+lengthPreamble;
                for(unsigned int spsIndex = 0; spsIndex < spsLength; spsIndex++)    {
                    
                    nalInfo.headerBytes.push_back(pRawSPSBytes[spsIndex]);
                    DEBUG_VTB_PRINT(printf(" 0x%02x", pRawSPSBytes[spsIndex] & 0x0ff));
                }
                DEBUG_VTB_PRINT(printf("\n"));
                
#ifdef RMH_USE_ADDTONALQUEUE
                m_nalQueueMutex.Lock();
                AddToNALQueue(nalInfo);
                m_nalQueueMutex.Unlock();
#else
                
                //See if dummy nalInfo entries must be pushed.
                //The dummy entries represent IDR or non_IDR NALs.                
                if((lastEntryPushed != initialLEPValue) && (nalNumber > lastEntryPushed + 1))  {
                    
                    unsigned int dummyCount = nalNumber - (lastEntryPushed + 1);
                    struct nalStreamInfo dummyNALInfo;
                    DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s SPS is in bitstream, pushing %d dummy entries onto prepend data queue\n", __FUNCTION__, dummyCount);)
                    
                    //push dummy entries equal to the number of non-header NALs
                    //between the non-header NAL being processed now,
                    //and the last time a non-header NAL was processed.
                    for (unsigned count = 0; count < dummyCount; count++) {
                        dummyNALInfo.nalNumber = lastEntryPushed + 1 + count;
                        dummyNALInfo.nalType = NAL_UT_SLICE;    //Just has to be different than SPS, PPS, SEI
                        m_prependDataQueue.push(dummyNALInfo);
                    }
                }
                m_prependDataQueue.push(nalInfo);
                lastEntryPushed = nalNumber;
#endif
            }
            break;
        }
        case NAL_UT_PPS:
        {
            
            //Add this PPS information to existing PPS information
            size = nalUnit->GetDataSize();
            DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s PPS size = %d, nal number = %d\n", __FUNCTION__, size, nalNumber);)
            hdr = GetPPS();
            id = m_Headers.m_PicParams.GetCurrentID();
            hdr->Resize(id, size + sizeof(start_code_prefix));
            memcpy(hdr->GetPointer(), start_code_prefix,  sizeof(start_code_prefix));
            memcpy(hdr->GetPointer() + sizeof(start_code_prefix), (Ipp8u*)nalUnit->GetDataPointer(), size);                   
            hdr->SetRBSPSize(size);   
         
            m_isHavePPS = true;
            if (m_isHaveSPS) {
                
                //Instantiate the VDA Decoder
                if(false == m_isVDAInstantiated)    {
                                        
                    H264VideoDecoderParams decoderParams;
                    TaskSupplier::GetInfo(&decoderParams);
                    RawHeader * pSPSHeader = GetSPS();
                    
                    struct nalStreamInfo nalInfo;
                    nalInfo.nalNumber = nalNumber;
                    nalInfo.nalType = nal_unit_type;

                    //Prepare the data for VDACreateDecoder
                    m_avcData.clear();
                    m_avcData.push_back((Ipp8u) 0x01);                            //Version
                    m_avcData.push_back(decoderParams.profile);                   //AVC Profile
                    
                    //14496-15 says that the next byte is defined exactly the same as the byte which occurs
                    //between the profile_IDC and level_IDC in a sequence parameter set (SPS).
                    const int theByteAfterProfile = 6;
                    m_avcData.push_back(*(pSPSHeader->GetPointer() + theByteAfterProfile));  //profile_compatibility
                    
                    m_avcData.push_back(decoderParams.level);                     //AVC Level
                    m_avcData.push_back((Ipp8u) 0xff);                            //Length of Size minus one and 6 reserved bits. (4-1 = 3)
                    m_avcData.push_back((Ipp8u) 0xe1);                            //Number of Sequence Parameter Sets (hard-coded to 1) and 3 reserved bits
                    
                    //Length of SPS
                    int spsLength = pSPSHeader->GetRBSPSize();
                    Ipp16u bigEndianSPSLength = htons(spsLength);
                    m_avcData.push_back((Ipp8u) (bigEndianSPSLength & 0x00ff));         //LSB of SPS Length (expressed in network order)
                    m_avcData.push_back((Ipp8u) ((bigEndianSPSLength >> 8) & 0x00ff));  //MSB of SPS Length (expressed in network order)
                    
                    Ipp32u bigEndianSPSLength32 = htonl(spsLength);
                    nalInfo.headerBytes.push_back((Ipp8u) (bigEndianSPSLength32 & 0x00ff));         //LSB of SPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSPSLength32 >> 8) & 0x00ff));  //MSB of SPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSPSLength32 >> 16) & 0x00ff));  //MSB of SPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianSPSLength32 >> 24) & 0x00ff));  //MSB of SPS Length (expressed in network order)
                    
                    //Copy raw bytes of SPS to this local spot (avcData)
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Data for initial SPS:", __FUNCTION__));                    
                    Ipp8u * pRawSPSBytes = pSPSHeader->GetPointer()+lengthPreamble;
                    for(unsigned int spsIndex = 0; spsIndex < spsLength; spsIndex++)    {
                        
                        m_avcData.push_back(pRawSPSBytes[spsIndex]);
                        nalInfo.headerBytes.push_back(pRawSPSBytes[spsIndex]);
                        DEBUG_VTB_PRINT(printf(" 0x%02x", pRawSPSBytes[spsIndex] & 0x0ff));
                    }
                    DEBUG_VTB_PRINT(printf("\n"));
                    
#ifdef USE_APPLEGVA
                    std::vector<Ipp8u> sps_ppsBytes;
                    unsigned int ppsIndex;
                    
                    sps_ppsBytes.clear();
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s AppleGVA Data for initial SPS:", __FUNCTION__));
                    Ipp8u * pAllRawSPSBytes = pSPSHeader->GetPointer();
                    for(unsigned int spsIndex = 0; spsIndex < spsLength+4; spsIndex++)    {
                        
                        sps_ppsBytes.push_back(pAllRawSPSBytes[spsIndex]);
                        DEBUG_VTB_PRINT(printf(" 0x%02x", pAllRawSPSBytes[spsIndex] & 0x0ff));
                    }
                    DEBUG_VTB_PRINT(printf("\n"));
                    ppsIndex = sps_ppsBytes.size();    //Save the offset to the start of PPS
                    
#endif
                    
                    m_avcData.push_back((Ipp8u) 1);                                     //Number of Picture Parameter Sets (hard-coded to 1 here)
                    m_ppsCountIndex = m_avcData.size() - 1;                             //Save the offset to the number of PPS
                    
                    //Length of PPS
                    int ppsLength = size;
                    Ipp16u bigEndianPPSLength = htons(ppsLength);
                    m_avcData.push_back((Ipp8u) (bigEndianPPSLength & 0x00ff));         //LSB of PPS Length (expressed in network order)
                    m_avcData.push_back((Ipp8u) ((bigEndianPPSLength >> 8) & 0x00ff));  //MSB of PPS Length (expressed in network order)
                    
                    Ipp32u bigEndianPPSLength32 = htonl(ppsLength);
                    nalInfo.headerBytes.push_back((Ipp8u) (bigEndianPPSLength32 & 0x00ff));         //LSB of PPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianPPSLength32 >> 8) & 0x00ff));  //MSB of PPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianPPSLength32 >> 16) & 0x00ff));  //MSB of PPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianPPSLength32 >> 24) & 0x00ff));  //MSB of PPS Length (expressed in network order)

                    //Copy raw bytes of PPS to this local spot (avcData)
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Data for initial PPS:", __FUNCTION__));
                    Ipp8u * pRawPPSBytes = hdr->GetPointer()+lengthPreamble;
                    for(unsigned int ppsIndex = 0; ppsIndex < ppsLength; ppsIndex++)    {
                        
                        m_avcData.push_back(pRawPPSBytes[ppsIndex]);
                        nalInfo.headerBytes.push_back(pRawPPSBytes[ppsIndex]);
                        DEBUG_VTB_PRINT(printf(" 0x%02x", pRawPPSBytes[ppsIndex] & 0x0ff));
                    }
                    DEBUG_VTB_PRINT(printf("\n"));
                    
                    DEBUG_VTB_PRINT(
                        printf(" VDATaskSupplier::%s avc data:", __FUNCTION__);
                        for(unsigned int index = 0; index < m_avcData.size(); index++)    {
                            printf(" 0x%02x", m_avcData[index] & 0x0ff);
                        }
                        printf("\n")
                    );
                    
#ifdef USE_APPLEGVA
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s AppleGVA Data for initial PPS:", __FUNCTION__));
                    Ipp8u * pAllRawPPSBytes = hdr->GetPointer();
                    for(unsigned int i = 0; i < ppsLength+4; i++)    {
                        
                        sps_ppsBytes.push_back(pAllRawPPSBytes[i]);
                        DEBUG_VTB_PRINT(printf(" 0x%02x", pAllRawPPSBytes[i] & 0x0ff));
                    }
                    DEBUG_VTB_PRINT(printf("\n"));
#endif
                    

#ifdef RMH_USE_ADDTONALQUEUE
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Initial SPS-PPS: nalNumber = %d\n", __FUNCTION__, nalNumber);)
                    
                    m_nalQueueMutex.Lock();
                    AddToNALQueue(nalInfo);
                    m_nalQueueMutex.Unlock();
#else
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Initial SPS-PPS: lastEntryPushed = %d, nalNumber = %d\n", __FUNCTION__, lastEntryPushed, nalNumber);)

                    //See if dummy nalInfo entries must be pushed.
                    //The dummy entries represent IDR or non_IDR NALs.
                    if((lastEntryPushed != initialLEPValue) && (nalNumber > lastEntryPushed + 1))  {
                        
                        unsigned int dummyCount = nalNumber - (lastEntryPushed + 1);
                        struct nalStreamInfo dummyNALInfo;
                        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s pushing %d dummy entries onto prepend data queue\n", __FUNCTION__, dummyCount);)
                        
                        //push dummy entries equal to the number of non-header NALs
                        //between the non-header NAL being processed now,
                        //and the last time a non-header NAL was processed. 
                        for (unsigned count = 0; count < dummyCount; count++) {
                            dummyNALInfo.nalNumber = lastEntryPushed + 1 + count;
                            dummyNALInfo.nalType = NAL_UT_SLICE;    //Just has to be different than SPS, PPS, SEI
                            m_prependDataQueue.push(dummyNALInfo);
                        }
                    }
                    m_prependDataQueue.push(nalInfo);
                    lastEntryPushed = nalNumber;
#endif  //RMH_USE_ADDTONALQUEUE
                    
                    Ipp8u avcData[1024];
                    memcpy(&avcData[0], m_avcData.data(), m_avcData.size());
                        
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s initial m_avcData.size() = %d\n", __FUNCTION__, m_avcData.size());)
                    CFDataRef avcDataRef;
                    avcDataRef = CFDataCreate(kCFAllocatorDefault, avcData, m_avcData.size()/*index*/);
                    if(NULL == avcDataRef)    {
                        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Error: could not create avcDataRef from avcData Ipp8u array\n", __FUNCTION__);)
                        throw h264_exception(UMC_ERR_DEVICE_FAILED);
                    }   
                    else    {
#ifdef USE_APPLEGVA
                        {
                            int gvaReturnValue;
                            
                            //Set the scan mode
                            m_gvaInformation.rbp_minus6680 = 0;
                            m_gvaInformation.rbp_minus6676 = 0;
                            
                            gvaReturnValue = m_pSetScanMode(m_gvaInformation.createAcceleratorParameter3, 2, &m_gvaInformation.rbp_minus6680);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetScanMode returned %d\n", __FUNCTION__, gvaReturnValue);)

                            //Create the main video bitstream pool
                            m_gvaInformation.rbp_minus6688 = 0x300000;    //maybe this can be a stand-alone local variable rather than a member of gvaInformation.
                            
                            gvaReturnValue = m_pCreateVideoBitstreamPool(m_gvaInformation.createAcceleratorParameter3, 0, &m_gvaInformation.rbp_minus6688);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s CreateVideoBitstreamPool returned %d\n", __FUNCTION__, gvaReturnValue);)
                            
                            if(gvaReturnValue == APPLEGVA_RETURN_PASS)
                            {
                                m_bVideoBitstreamPoolExists = true;
                            }
                           
                            //Create the main video data path
                            struct _prepareStructure prepareFunctionsData;
                            
                            prepareFunctionsData.pMallocBy_prepareFunctions = prepareFunctions(0, &prepareFunctionsData.valueInitializedBy_prepareFunctions, 0, 8, 0, 0);
                            
                            prepareFunctionsData.r15_plus16 = 5;
                            prepareFunctionsData.r15_plus24 = m_gvaInformation.arrayPointersOtherCallbackIdentifiers;

                            gvaReturnValue = m_pCreateMainVideoDataPath(m_gvaInformation.createAcceleratorParameter3, 0, &prepareFunctionsData);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s CreateMainVideoDataPath returned %d\n", __FUNCTION__, gvaReturnValue);)
                                                        
                            //Set the description of the decode by calling Function 20 seven times
                            //Set the width
                            struct _resolution
                            {
                                uint32_t width;
                                uint32_t height;
                            } resolution;
                            
                            resolution.width = decoderParams.m_fullSize.width;
                            resolution.height = decoderParams.m_fullSize.height;
                            m_gvaInformation.width = resolution.width;
                            m_gvaInformation.height = resolution.height;
                            
                            gvaReturnValue = m_pSetDecodeDescription(m_gvaInformation.createAcceleratorParameter3, 0, 1, SETDECODE_WIDTH, &resolution);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetDecodeDescription of width = %d returned %d\n", __FUNCTION__, resolution.width, gvaReturnValue);)
                            
                            //Set the height
                            gvaReturnValue = m_pSetDecodeDescription(m_gvaInformation.createAcceleratorParameter3, 0, 1, SETDECODE_HEIGHT, &resolution);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetDecodeDescription of height = %d returned %d\n", __FUNCTION__, resolution.height, gvaReturnValue);)
                            
                            //Set the stream frame rate
                            float frameRate = 29.97f;
                            gvaReturnValue = m_pSetDecodeDescription(m_gvaInformation.createAcceleratorParameter3, 0, 1, SETDECODE_FRAMERATE, &frameRate);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetDecodeDescription of frame rate = %f returned %d\n", __FUNCTION__, frameRate, gvaReturnValue);)
                            
                            //Set the decoder's ouput order
                            //Note: 0 = output is in display order, 1 = output is in decode order
                            uint32_t decoderOutputOrder = APPLEGVA_OUTPUT_DECODE_ORDER;
                            gvaReturnValue = m_pSetDecodeDescription(m_gvaInformation.createAcceleratorParameter3, 0, 1, SETDECODE_OUTPUT_ORDER, &decoderOutputOrder);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetDecodeDescription of decoder's output order to display order returned %d\n", __FUNCTION__, gvaReturnValue);)
                            
                            //Set the number of decode buffers
                            uint32_t numberOfDecodeBuffers = 8;
                            gvaReturnValue = m_pSetDecodeDescription(m_gvaInformation.createAcceleratorParameter3, 0, 1, SETDECODE_BUFFER_COUNT, &numberOfDecodeBuffers);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetDecodeDescription of number of decode buffers = %d returned %d\n", __FUNCTION__, numberOfDecodeBuffers, gvaReturnValue);)
                            
                            //Set the value for Enable FMBS
                            uint32_t enableFMBSValue = 0;
                            gvaReturnValue = m_pSetDecodeDescription(m_gvaInformation.createAcceleratorParameter3, 0, 1, SETDECODE_ENABLE_FMBS, &enableFMBSValue);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetDecodeDescription of enable FMBS returned %d\n", __FUNCTION__, gvaReturnValue);)
                            
                            //Set the value for Enable FMSS
                            uint32_t enableFMSSValue = 0;
                            gvaReturnValue = m_pSetDecodeDescription(m_gvaInformation.createAcceleratorParameter3, 0, 1, SETDECODE_ENABLE_FMSS, &enableFMSSValue);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetDecodeDescription of enable FMSS returned %d\n", __FUNCTION__, gvaReturnValue);)
                            
                            //Give the SPS and PPS to AppleGVA
                            NALDescriptor spsNAL, ppsNAL;
                            NALDescriptor * nalDescriptors[2];
                            
                            nalDescriptors[0] = &spsNAL;
                            nalDescriptors[1] = &ppsNAL;
                                                        
                            gvaReturnValue = m_pGetNALBufferAddress(m_gvaInformation.createAcceleratorParameter3, 0, sps_ppsBytes.size(), &m_pNALBuffer);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s GetNALBufferAddress returned %d, NAL buffer pointer = %p\n", __FUNCTION__, gvaReturnValue, m_pNALBuffer);)
                            if (gvaReturnValue == APPLEGVA_RETURN_PASS) {
                                if (m_pNALBuffer == NULL) {
                                    DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s GetNALBufferAddress returned NULL buffer pointer\n", __FUNCTION__);)
                                    throw h264_exception(UMC_ERR_DEVICE_FAILED);
                                }
                                
                                //Move the SPS and PPS NAL data to GVA's NAL buffer
                                memcpy(m_pNALBuffer, &sps_ppsBytes[0], sps_ppsBytes.size());
                                
                                //Initialize the SPS and PPS NAL descriptors
                                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s spsLength+4 = %d, ppsLength+4 = %d, ppsIndex = %d\n", __FUNCTION__, spsLength+4, ppsLength+4, ppsIndex);)
                                spsNAL.sizeNAL = spsLength+4;
                                spsNAL.pNALData = &m_pNALBuffer[0];
                                spsNAL.reserved = 0;
                                ppsNAL.sizeNAL = ppsLength+4;
                                ppsNAL.pNALData = &m_pNALBuffer[ppsIndex];
                                ppsNAL.reserved = 0;
                            }
                            else    {
                                
                                //Could not get NAL buffer address from AppleGVA
                                throw h264_exception(UMC_ERR_DEVICE_FAILED);
                            }
                            
                            gvaReturnValue = m_pSetNALBufferReady(m_gvaInformation.createAcceleratorParameter3, m_gvaIndex, 0);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetNALBufferReady returned %d\n", __FUNCTION__, gvaReturnValue);)
                            if (gvaReturnValue == APPLEGVA_RETURN_FAIL) {
                                throw h264_exception(UMC_ERR_DEVICE_FAILED);
                            }
                            
                            gvaReturnValue = m_pSetNALDescriptors(m_gvaInformation.createAcceleratorParameter3, m_gvaIndex, 0, 2, nalDescriptors);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetNALDescriptors returned %d\n", __FUNCTION__, gvaReturnValue);)
                            if (gvaReturnValue == APPLEGVA_RETURN_FAIL) {
                                throw h264_exception(UMC_ERR_DEVICE_FAILED);
                            }
                            
                            NALDescriptor * pUnknown;
                            pUnknown = NULL;
                            
                            gvaReturnValue = m_pFunction15(m_gvaInformation.createAcceleratorParameter3, m_gvaIndex, 1, &pUnknown);
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Function15 returned %d, pUnknown = %p\n", __FUNCTION__, gvaReturnValue, pUnknown);)
                            if (gvaReturnValue == APPLEGVA_RETURN_FAIL) {
                                throw h264_exception(UMC_ERR_DEVICE_FAILED);
                            }
                            
                            m_isVDAInstantiated = TRUE;                           
                        }
#else
                        {
                        
                            OSStatus returnValue;
                            
                            //Create the video decoder specification
                            CFMutableDictionaryRef avcDictionary;
                            avcDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                      1,
                                                                      &kCFTypeDictionaryKeyCallBacks,
                                                                      &kCFTypeDictionaryValueCallBacks);
                            
                            CFDictionarySetValue(avcDictionary, CFSTR("avcC"), avcDataRef);
                            
                            CFMutableDictionaryRef decoderConfigurationDictionary;
                            decoderConfigurationDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                                       11-1,
                                                                                       &kCFTypeDictionaryKeyCallBacks,
                                                                                       &kCFTypeDictionaryValueCallBacks);
                            CFDictionarySetValue(decoderConfigurationDictionary, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, avcDictionary);
                            
                            CFNumberRef qtpVersion;
                            short versionValue = 0;
                            qtpVersion = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &versionValue);
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("Version"), qtpVersion);//0412
                            
                            CFStringRef qtpFormatName = CFSTR("'avc1'");
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("FormatName"), qtpFormatName);//0412
                            
                            CFNumberRef qtpRevisionLevel;
                            short revisionLevelValue = 0;
                            qtpRevisionLevel = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &revisionLevelValue);
                           CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("RevisionLevel"), qtpRevisionLevel);//0412 
                            
                            CFNumberRef qtpTemporalQuality;
                            int temporalQualityValue = 0;
                            qtpTemporalQuality = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &temporalQualityValue);
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("TemporalQuality"), qtpTemporalQuality);//0412
                            
                            CFNumberRef qtpSpatialQuality;
                            int spatialQualityValue = 0;
                            qtpSpatialQuality = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &spatialQualityValue);
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("SpatialQuality"), qtpSpatialQuality);//0412
                            
                            CFNumberRef qtpDepth;
                            short depthValue = 24;
                            qtpDepth = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &depthValue);
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("Depth"), qtpDepth);//0412
                            
                            //CVFieldCount is new 0412
/*                            CFNumberRef cvFieldCount;
                            int cvFieldCountValue = 1;
                            cvFieldCount = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &cvFieldCountValue);
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("CVFieldCount"), cvFieldCount);*/
                            
                            CFStringRef qtpChromaLocationTopField = CFSTR("Left");
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("CVImageBufferChromaLocationTopField"), qtpChromaLocationTopField);
                            
                            CFStringRef qtpChromaLocationBottomField = CFSTR("Left");
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("CVImageBufferChromaLocationBottomField"), qtpChromaLocationBottomField);
                            
                            CFDictionarySetValue(decoderConfigurationDictionary, CFSTR("FullRangeVideo"), kCFBooleanFalse);
                            //end of QTP configuration
                            
                            //Describe the video format description
                            returnValue = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                                                         'avc1',
                                                                         decoderParams.m_fullSize.width,    //info.clip_info.width,
                                                                         decoderParams.m_fullSize.height,   //info.clip_info.height,
                                                                         decoderConfigurationDictionary,
                                                                         &m_sourceFormatDescription);
                            if(noErr != returnValue)
                            {
                                DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s error %d creating source format description.\n", __FUNCTION__, returnValue);)
                                throw h264_exception(UMC_ERR_DEVICE_FAILED);
                            }
                            
                            //Using this because Quick Time Player specified it
                            CFMutableDictionaryRef videoDecoderSpecificationDictionary;
                            videoDecoderSpecificationDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                                           1,
                                                                                           &kCFTypeDictionaryKeyCallBacks,
                                                                                           &kCFTypeDictionaryValueCallBacks);
                            

    #if 1
                            CFDictionarySetValue(videoDecoderSpecificationDictionary, CFSTR("EnableHardwareAcceleratedVideoDecoder"), kCFBooleanTrue);
    #else
                            if((decoderParams.m_fullSize.height < 320) || (decoderParams.m_fullSize.width < 480))    {
                                CFDictionarySetValue(videoDecoderSpecificationDictionary, CFSTR("EnableHardwareAcceleratedVideoDecoder"), kCFBooleanTrue);
                            }
                            else    {
                                CFDictionarySetValue(videoDecoderSpecificationDictionary, CFSTR("RequireHardwareAcceleratedVideoDecoder"), kCFBooleanTrue);
                            }

                            /*CFNumberRef hardwareAcceleration;
                            int hardwareAccelerationValue = 1;
                            hardwareAcceleration = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &hardwareAccelerationValue);
                            CFDictionarySetValue(videoDecoderSpecificationDictionary, kCVPixelBufferIOSurfacePropertiesKey, hardwareAcceleration);*/
    #endif
                            
                            //Create the destination image buffer attributes
                            CFMutableDictionaryRef destinationImageBufferDictionary;
    #ifdef RMH_USE_VTB_0412
                            
                            
                            destinationImageBufferDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                                         6-4,   //0412 changed -3 to -4. prior to 0412: deleted three entries: width, height, and IOSurfaceCoreAnimationCompatibility
                                                                                         &kCFTypeDictionaryKeyCallBacks,
                                                                                         &kCFTypeDictionaryValueCallBacks);
                            
                            CFNumberRef height;
                            CFNumberRef width;
                            
                            height = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &decoderParams.info.clip_info.height);
                            width = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &decoderParams.info.clip_info.width);
                            
                            CFNumberRef pixelFormats[3];
                            
    //0412                        OSType cvPixelFormatType = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange; //kCVPixelFormatType_420YpCbCr8Planar;
                            OSType cvPixelFormatType = kCVPixelFormatType_420YpCbCr8Planar;
                            pixelFormats[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &cvPixelFormatType);
                            cvPixelFormatType = ' ';
    //0412                        pixelFormats[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &cvPixelFormatType);
    //0412                        CFArrayRef pixelFormatArray = CFArrayCreate(kCFAllocatorDefault, (const void * *) pixelFormats, 2, &kCFTypeArrayCallBacks);
                            CFArrayRef pixelFormatArray = CFArrayCreate(kCFAllocatorDefault, (const void * *) pixelFormats, 1, &kCFTypeArrayCallBacks);
                            
                            
                            CFMutableDictionaryRef ioSurfacePropertiesDictonary;
                            ioSurfacePropertiesDictonary = CFDictionaryCreateMutable(kCFAllocatorDefault, // our empty IOSurface properties dictionary
                                                                                     0,
                                                                                     &kCFTypeDictionaryKeyCallBacks,
                                                                                     &kCFTypeDictionaryValueCallBacks);
                            
    //0412                        CFDictionarySetValue(destinationImageBufferDictionary, CFSTR("OpenGLCompatibility"), kCFBooleanTrue);
                            CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferPixelFormatTypeKey, pixelFormatArray);
    //                        CFDictionarySetValue(destinationImageBufferDictionary, CFSTR("IOSurfaceCoreAnimationCompatibility"), kCFBooleanTrue);
                            CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferIOSurfacePropertiesKey, ioSurfacePropertiesDictonary);
    //                        CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferWidthKey, width);
    //                        CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferHeightKey, height);
                            
    #else
                            destinationImageBufferDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                             6-3,   //0412 changed -3 to -4. prior to 0412: deleted three entries: width, height, and IOSurfaceCoreAnimationCompatibility
                                                                             &kCFTypeDictionaryKeyCallBacks,
                                                                             &kCFTypeDictionaryValueCallBacks);
                            
                            CFNumberRef height;
                            CFNumberRef width;
                            
                            height = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &decoderParams.info.clip_info.height);
                            width = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &decoderParams.info.clip_info.width);
                            
                            CFNumberRef pixelFormats[3];
                            
    //0412                        OSType cvPixelFormatType = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange; //kCVPixelFormatType_420YpCbCr8Planar;
                            OSType cvPixelFormatType = kCVPixelFormatType_420YpCbCr8Planar;
                            pixelFormats[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &cvPixelFormatType);
                            cvPixelFormatType = ' ';
                            pixelFormats[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &cvPixelFormatType);
                            CFArrayRef pixelFormatArray = CFArrayCreate(kCFAllocatorDefault, (const void * *) pixelFormats, 2, &kCFTypeArrayCallBacks);
                            
                            
                            CFMutableDictionaryRef ioSurfacePropertiesDictonary;
                            ioSurfacePropertiesDictonary = CFDictionaryCreateMutable(kCFAllocatorDefault, // our empty IOSurface properties dictionary
                                                                              0,
                                                                              &kCFTypeDictionaryKeyCallBacks,
                                                                              &kCFTypeDictionaryValueCallBacks);
                            
                            CFDictionarySetValue(destinationImageBufferDictionary, CFSTR("OpenGLCompatibility"), kCFBooleanTrue);//0412, had been removed 
                            CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferPixelFormatTypeKey, pixelFormatArray);
    //                        CFDictionarySetValue(destinationImageBufferDictionary, CFSTR("IOSurfaceCoreAnimationCompatibility"), kCFBooleanTrue);
                            CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferIOSurfacePropertiesKey, ioSurfacePropertiesDictonary);
    //                        CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferWidthKey, width);
    //                        CFDictionarySetValue(destinationImageBufferDictionary, kCVPixelBufferHeightKey, height);
    #endif  //RMH_USE_VTB_0412
                            
                            //Create the video toolbox decompression session
                            
                            VTDecompressionOutputCallbackRecord decompressionCallbackRecord;
                            decompressionCallbackRecord.decompressionOutputCallback = (VTDecompressionOutputCallback) ((TaskBrokerSingleThreadVDA *)m_pTaskBroker)->decoderOutputCallback2;
                            decompressionCallbackRecord.decompressionOutputRefCon = (void *) m_pTaskBroker;
                            
                            returnValue = CALL_VTB_FUNC(VTDecompressionSessionCreate)(/*kCFAllocatorSystemDefault,*/ kCFAllocatorDefault,
                                                                       m_sourceFormatDescription,
                                                                       videoDecoderSpecificationDictionary,
                                                                       destinationImageBufferDictionary,
                                                                       &decompressionCallbackRecord,
                                                                       &m_decompressionSession);

                           if(noErr != returnValue)
                           {
                               DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s error %d returned by VTDecompressionSessionCreate.\n", __FUNCTION__, returnValue);)
                               throw h264_exception(UMC_ERR_DEVICE_FAILED);
                           }
                            else
                            {
                                m_isVDAInstantiated = TRUE;
                                DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s Video Toolbox decompression session created ok\n", __FUNCTION__);)
                                CALL_VTB_FUNC(VTDecompressionSessionWaitForAsynchronousFrames)(m_decompressionSession);
                                CALL_VTB_FUNC(VTDecompressionSessionFinishDelayedFrames)(m_decompressionSession);
                            }
                        }
#endif //USE_APPLEGVA
                    }
                }
                else    {
                    
                    //VDA decoder is already instantiated, is this a new SPS-PPS sequence?
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s appending avcC data because VTB decoder is instantiated, PPS is in bitstream\n", __FUNCTION__);)
#ifdef RMH_USE_ADDTONALQUEUE
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s nalNumber = %d\n", __FUNCTION__, nalNumber);)
#else
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s lastEntryPushed = %d, nalNumber = %d\n", __FUNCTION__, lastEntryPushed, nalNumber);)
#endif

                    VideoDecoderParams decoderParams;
                    TaskSupplier::GetInfo(&decoderParams);
                    
                    struct nalStreamInfo nalInfo;
                    nalInfo.nalNumber = nalNumber;
                    nalInfo.nalType = nal_unit_type;
                    
                    //Increment the PPS count, and then append this PPS
//                    m_avcData[m_ppsCountIndex]++;
                    
                    //Length of PPS
                    int ppsLength = size;
                    Ipp16u bigEndianPPSLength = htons(ppsLength);
//                    m_avcData.push_back((Ipp8u) (bigEndianPPSLength & 0x00ff));         //LSB of PPS Length (expressed in network order)
//                    m_avcData.push_back((Ipp8u) ((bigEndianPPSLength >> 8) & 0x00ff));  //MSB of PPS Length (expressed in network order)

                    Ipp32u bigEndianPPSLength32 = htonl(ppsLength);
                    nalInfo.headerBytes.push_back((Ipp8u) (bigEndianPPSLength32 & 0x00ff));         //LSB of PPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianPPSLength32 >> 8) & 0x00ff));  //MSB of PPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianPPSLength32 >> 16) & 0x00ff));  //MSB of PPS Length (expressed in network order)
                    nalInfo.headerBytes.push_back((Ipp8u) ((bigEndianPPSLength32 >> 24) & 0x00ff));  //MSB of PPS Length (expressed in network order)
                    
                    //Copy raw bytes of PPS
                    Ipp8u * pRawPPSBytes = hdr->GetPointer()+lengthPreamble;
                    for(unsigned int ppsIndex = 0; ppsIndex < ppsLength; ppsIndex++)    {
                        
                        nalInfo.headerBytes.push_back(pRawPPSBytes[ppsIndex]);
                    }
                    
#ifdef RMH_USE_ADDTONALQUEUE
                    m_nalQueueMutex.Lock();
                    AddToNALQueue(nalInfo);
                    m_nalQueueMutex.Unlock();
#else
                    //See if dummy nalInfo entries must be pushed.
                    //The dummy entries represent IDR or non_IDR NALs.
                    if((lastEntryPushed != initialLEPValue) && (nalNumber == (lastEntryPushed + 1)))  {
                        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SPS-PPS-PPS pattern detected\n", __FUNCTION__);)
                    }
                    
                    if((lastEntryPushed != initialLEPValue) && (nalNumber > lastEntryPushed + 1))  {
                        
                        unsigned int dummyCount = nalNumber - (lastEntryPushed + 1);
                        struct nalStreamInfo dummyNALInfo;
                        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s pushing %d dummy entries onto prepend data queue\n", __FUNCTION__, dummyCount);)
                        
                        //push dummy entries equal to the number of non-header NALs
                        //between the non-header NAL being processed now,
                        //and the last time a non-header NAL was processed.
                        for (unsigned count = 0; count < dummyCount; count++) {
                            dummyNALInfo.nalNumber = lastEntryPushed + 1 + count;
                            dummyNALInfo.nalType = NAL_UT_SLICE;    //Just has to be different than SPS, PPS, SEI
                            m_prependDataQueue.push(dummyNALInfo);
                        }
                    }
                    m_prependDataQueue.push(nalInfo);
                    lastEntryPushed = nalNumber;
#endif //RMH_USE_ADDTONALQUEUE
                    
//                    Ipp8u avcData[1024];
//                    memcpy(&avcData[0], m_avcData.data(), m_avcData.size());
                    
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s updated m_avcData.size() = %d\n", __FUNCTION__, m_avcData.size());)

                }   //PPS is already in bitstream
            }
            else    {
                
                //Odd, the NAL is a PPS but we don't have the SPS yet!
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s WARNING, have PPS but have not seen SPS yet\n", __FUNCTION__));
            }
            break;
        }
        case NAL_UT_SEI:
        {
            size = nalUnit->GetDataSize();
            DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s SEI size = %d, nal number = %d\n", __FUNCTION__, size, nalNumber);)
            break;
        }
        default:
            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s unhandled nal type = %d\n", __FUNCTION__, nal_unit_type);)
            break;
    }
    
    MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
    if ((NAL_Unit_Type)pMediaDataEx->values[0] == NAL_UT_SPS && m_firstVideoParams.mfx.FrameInfo.Width)
    {
        H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();
        
        if (currSPS)
        {
            if (m_firstVideoParams.mfx.FrameInfo.Width != (currSPS->frame_width_in_mbs * 16) || 
                m_firstVideoParams.mfx.FrameInfo.Height != (currSPS->frame_height_in_mbs * 16) ||
                (m_firstVideoParams.mfx.CodecLevel && m_firstVideoParams.mfx.CodecLevel < currSPS->level_idc))
            {
                return UMC_NTF_NEW_RESOLUTION;
            }
        }
        
        return UMC_WRN_REPOSITION_INPROGRESS;
    }

    return UMC_OK;   
}
    
bool VDATaskSupplier::GetFrameToDisplay(MediaData *dst, bool force)
{
    // Perform output color conversion and video effects, if we didn't
    // already write our output to the application's buffer.
    VideoData *pVData = DynamicCast<VideoData> (dst);
    if (!pVData)
        return false;
    
    m_pLastDisplayed = 0;
    
    H264DecoderFrame * pFrame = 0;
    
    do
    {
        pFrame = GetFrameToDisplayInternal(force);
        if (!pFrame || !pFrame->IsDecoded())
        {
            return false;
        }
        
        PostProcessDisplayFrame(dst, pFrame);
        
        if (pFrame->IsSkipped())
        {
            SetFrameDisplayed(pFrame->PicOrderCnt(0, 3));
        }
    } while (pFrame->IsSkipped());
    
    m_pLastDisplayed = pFrame;
    pVData->SetInvalid(pFrame->GetError());
    
    VideoData data;
    
    InitColorConverter(pFrame, &data, 0);
    
    if (m_pPostProcessing->GetFrame(&data, pVData) != UMC_OK)
    {
        pFrame->setWasOutputted();
        pFrame->setWasDisplayed();
        return false;
    }
    
    pVData->SetDataSize(pVData->GetMappingSize());
    
    pFrame->setWasOutputted();
    pFrame->setWasDisplayed();
    return true;
}
    
#ifdef USE_APPLEGVA
    
/*void printFrameContext(std::deque<struct _decodeFrameContext> & callbackContextDeque, unsigned int index)
{
    
    if (callbackContextDeque.size()) {
        struct _decodeFrameContext & frameContext = callbackContextDeque[index];
        DEBUG_VTB_PRINT(printf("%s: frame number = %d, frame type = %d\n", __FUNCTION__, frameContext.frameNumber, frameContext.frameType);)
    }
}*/
    
Status VDATaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    
    static unsigned int counter = 0;

    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Entry, counter = %d\n", __FUNCTION__, ++counter);)
    
    if(NULL == pFrame)  {
        return UMC_OK;
    }
    
    H264DecoderFrameInfo * sliceInfo = pFrame->GetAU(field);
    
    //check for slice groups, they are not supported by HD graphics
    if (sliceInfo->GetSlice(0)->IsSliceGroups())
    {
        DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s EXCEPTION slice groups are not supported\n", __FUNCTION__);)
        throw h264_exception(UMC_ERR_UNSUPPORTED);
    }
    
    if (sliceInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED) {
        return UMC_OK;
    }
    
    
    Status baseStatus;
    baseStatus = TaskSupplier::CompleteFrame(pFrame, field);
    if (UMC_OK != baseStatus) {
        return baseStatus;
    }
        
    OSStatus status;
    static Ipp32s frameCounter = 0;
    
    Ipp32s frameType = pFrame->m_FrameType;
    Ipp32s frame_num = pFrame->m_FrameNum;
    
    char * pFrameTypeName;
    switch (frameType) {
        case 1:
            pFrameTypeName = "I Frame";
            break;
        case 2:
            pFrameTypeName = "P Frame";
            break;
        case 3:
            pFrameTypeName = "B Frame";
            break;
        default:
            pFrameTypeName = "Unknown Frame Type";
            break;
    }
    
    Ipp32s nalType;
    Ipp32s index = pFrame->m_index;
    
    DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s slice count = %d, field = %d, IsField = %d, IsBottom = %d\n", __FUNCTION__, sliceInfo->GetSliceCount(), field, sliceInfo->IsField(), sliceInfo->IsBottom());)
    
    unsigned int totalSizeEncodedBytes = 0;
    unsigned int gvaReturnValue;
    
    static Ipp8u start_code_prefix[] = {0, 0, 0, 1};
    const int lengthStartCodePrefix = sizeof(start_code_prefix);
    
    //Frame or Field?
    if(sliceInfo->IsField())  {
        
        Ipp8u *pNalUnit;    //ptr to first byte of start code
        Ipp32u NalUnitSize; // size of NAL unit in bytes
        
        char pFieldType[16];
        if (sliceInfo->IsBottom()) {
            strcpy(pFieldType, "bottom");
        }
        else    {
            strcpy(pFieldType, "top");
        }
        
        if(m_sizeField)    {
            
            //Combine previously cached field with this field and give it to the VTB decoder.
            //Make sure that the frames match.
            if(m_pFieldFrame == pFrame)    {
                
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s recovered cached field of frame_num %d, size = %d\n", __FUNCTION__, frame_num, m_sizeField);)
                
                //Increment the byte count
                totalSizeEncodedBytes = m_sizeField;
                
                //Anything to prepend, such an additional/new PPS?
                if (!m_prependDataQueue.empty()) {
                    
                    struct nalStreamInfo nalInfo;
                    
                    do {
                        
                        //Get the entry at the front
                        nalInfo = m_prependDataQueue.front();
                        m_prependDataQueue.pop();
                        
                        if (nalInfo.nalType != NAL_UT_SLICE) {
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s prepending data for frame_num %d, size = %d\n", __FUNCTION__, frame_num, nalInfo.headerBytes.size());)
                            for(unsigned int prependIndex = 0; prependIndex < nalInfo.headerBytes.size(); prependIndex++)    {
                                m_nals.push_back(nalInfo.headerBytes[prependIndex]);
                            }
                            totalSizeEncodedBytes += nalInfo.headerBytes.size();
                        }
                    } while (!m_prependDataQueue.empty() && (nalInfo.nalType != NAL_UT_SLICE));
                }
                
                //Put the second field into the nals vector
                for (Ipp32u i = 0; i < sliceInfo->GetSliceCount(); i++)
                {
                    H264Slice *pSlice = sliceInfo->GetSlice(i);
                    
                    pSlice->GetBitStream()->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
                    
                    //resize the vector if necessary
                    if (m_nals.size() < totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int)) {
                        m_nals.resize(totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int));
                    }
                    
                    //copy the nal data into the buffer
                    memcpy(&m_nals[totalSizeEncodedBytes+sizeof(unsigned int)], pNalUnit, NalUnitSize);
                    *((int * ) &m_nals[totalSizeEncodedBytes]) = htonl(NalUnitSize);
                    
                    //Increment the byte count
                    totalSizeEncodedBytes += NalUnitSize + sizeof(unsigned int);
                }
                nalType = m_nals[m_sizeField+sizeof(unsigned int)] & NAL_UNITTYPE_BITS;
                
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s second field, %s (%d), NAL type %d, size = %d, size of both fields of frame_num %d = %d\n", __FUNCTION__, pFrameTypeName, frameType, nalType, totalSizeEncodedBytes - m_sizeField, frame_num, totalSizeEncodedBytes);)
                
                //Reset the top field information.
                m_sizeField = 0;
                m_pFieldFrame = NULL;
            }
            else    {
                
                //Error, the frames are different.
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Failure, frames of two consecutive fields are different\n", __FUNCTION__);)
                return  MFX_ERR_ABORTED;
            }
        }
        else    {
            
            //This is the first field of this frame.
            //Cache this field, do not give it to the VTB decoder now.
            m_nals.clear();
            totalSizeEncodedBytes = 0;
            
            //Anything to prepend, such an additional/new PPS?
            if (!m_prependDataQueue.empty()) {
                
                struct nalStreamInfo nalInfo;
                
                do {
                    
                    //Get the entry at the front
                    nalInfo = m_prependDataQueue.front();
                    m_prependDataQueue.pop();
                    
                    if (nalInfo.nalType != NAL_UT_SLICE) {
                        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s prepending data for frame_num %d, size = %d\n", __FUNCTION__, frame_num, nalInfo.headerBytes.size());)
                        for(unsigned int prependIndex = 0; prependIndex < nalInfo.headerBytes.size(); prependIndex++)    {
                            m_nals.push_back(nalInfo.headerBytes[prependIndex]);
                        }
                        totalSizeEncodedBytes += nalInfo.headerBytes.size();
                    }
                } while (!m_prependDataQueue.empty() && (nalInfo.nalType != NAL_UT_SLICE));
            }
            
            char pFieldType[16];
            
            if (sliceInfo->IsBottom()) {
                strcpy(pFieldType, "bottom");
            }
            else    {
                strcpy(pFieldType, "top");
            }
            
            for (Ipp32u i = 0; i < sliceInfo->GetSliceCount(); i++)
            {
                
                H264Slice *pSlice = sliceInfo->GetSlice(i);
                
                pSlice->GetBitStream()->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
                
                //resize the vector if necessary
                if (m_nals.size() < totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int)) {
                    m_nals.resize(totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int));
                }
                
                //copy the nal data into the buffer
                memcpy(&m_nals[totalSizeEncodedBytes+sizeof(unsigned int)], pNalUnit, NalUnitSize);
                *((int * ) &m_nals[totalSizeEncodedBytes]) = htonl(NalUnitSize);
                
                //Increment the byte count
                totalSizeEncodedBytes += NalUnitSize + sizeof(unsigned int);
            }
            m_sizeField = totalSizeEncodedBytes;
            m_pFieldFrame = pFrame;
            nalType = m_nals[sizeof(unsigned int)] & NAL_UNITTYPE_BITS;
            
            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s cached %s field of frame_num %d, %s (%d), NAL type %d, size = %d, %d slices\n", __FUNCTION__,  pFieldType, frame_num, pFrameTypeName, frameType, nalType, m_sizeField, sliceInfo->GetSliceCount());)
            
            nalType = m_nals[sizeof(unsigned int)] & NAL_UNITTYPE_BITS;
            
            return UMC_OK;
        }
    }
    else    {
        
        //It is a frame, not a field
        m_nals.clear();
        totalSizeEncodedBytes = 0;
        
        //Anything to prepend, such an additional/new PPS?
/*        if (!m_prependDataQueue.empty()) {
            
            struct nalStreamInfo nalInfo;
            
            do {
                
                //Get the entry at the front
                nalInfo = m_prependDataQueue.front();
                m_prependDataQueue.pop();
                
                if (nalInfo.nalType != NAL_UT_SLICE) {
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s prepending data for frame_num %d, size = %d\n", __FUNCTION__, frame_num, nalInfo.headerBytes.size());)
                    for(unsigned int prependIndex = 0; prependIndex < nalInfo.headerBytes.size(); prependIndex++)    {
                        m_nals.push_back(nalInfo.headerBytes[prependIndex]);
                    }
                    totalSizeEncodedBytes += nalInfo.headerBytes.size();
                }
            } while (!m_prependDataQueue.empty() && (nalInfo.nalType != NAL_UT_SLICE));
        }*/
        
        struct _sliceInformation
        {
            H264Slice *pSlice;
            Ipp8u *pNalUnit;    //ptr to first byte of start code
            Ipp32u NalUnitSize; // size of NAL unit in bytes
            NALDescriptor * pNALDescriptor;
        };
        
        unsigned int nalSizeTally = 0;
        unsigned int sliceCount;
        
        sliceCount = sliceInfo->GetSliceCount();
        
        struct _sliceInformation sliceInformation[sliceCount];
        NALDescriptor * nalDescriptors[sliceCount];
        
        //Calculate the total size of all NALs in this frame (and get pointers to their data)
        for (Ipp32u i = 0; i < sliceCount; i++)
        {            
            
            sliceInformation[i].pSlice = sliceInfo->GetSlice(i);
            sliceInformation[i].pSlice->GetBitStream()->GetOrg((Ipp32u**)&sliceInformation[i].pNalUnit, &sliceInformation[i].NalUnitSize);
            if (sliceInformation[i].pSlice->IsField()) {
                DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s slice %d of frame %d IsField = %d, slice with field\n", __FUNCTION__, i, frame_num, sliceInfo->IsField());)
            }
            nalSizeTally += sliceInformation[i].NalUnitSize + lengthStartCodePrefix;
        }
        
        //Get a buffer address from AppleGVA, then copy all the slices to that buffer. 
        gvaReturnValue = m_pGetNALBufferAddress(m_gvaInformation.createAcceleratorParameter3, 0, nalSizeTally, &m_pNALBuffer);
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s GetNALBufferAddress returned %d, NAL buffer pointer = %p\n", __FUNCTION__, gvaReturnValue, m_pNALBuffer);)
        if (gvaReturnValue == APPLEGVA_RETURN_PASS) {
            if (m_pNALBuffer == NULL) {
                DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s GetNALBufferAddress returned NULL buffer pointer\n", __FUNCTION__);)
                return MFX_ERR_ABORTED;
            }
            
            //Copy all the slices to the buffer
            for (Ipp32u i = 0; i < sliceCount; i++)
            {
                
                //Move the NAL data to GVA's NAL buffer
                memcpy(m_pNALBuffer, &start_code_prefix[0], lengthStartCodePrefix);
                memcpy(&m_pNALBuffer[lengthStartCodePrefix], sliceInformation[i].pNalUnit, sliceInformation[i].NalUnitSize);
                DEBUG_VTB_PRINT(printNALData(m_pNALBuffer, 64);)
                
                //Initialize the NAL descriptor
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s NAL length = %d\n", __FUNCTION__, sliceInformation[i].NalUnitSize+lengthStartCodePrefix);)
                nalDescriptors[i] = m_NALDescriptorPool[m_poolIndex];
                nalDescriptors[i]->sizeNAL = sliceInformation[i].NalUnitSize+lengthStartCodePrefix;
                nalDescriptors[i]->pNALData = &m_pNALBuffer[0];

                //Increment the provided pointer
                m_pNALBuffer += sliceInformation[i].NalUnitSize + lengthStartCodePrefix;
                
                //Next NALDesriptor
                m_poolIndex++;
                m_poolIndex &= m_poolIndexMask;
            }
        }
        else    {
            
            //Could not get NAL buffer address from AppleGVA
            return MFX_ERR_ABORTED;
        }
            
        //Tell AppleGVA that the buffer is ready
        gvaReturnValue = m_pSetNALBufferReady(m_gvaInformation.createAcceleratorParameter3, m_gvaIndex, 0);
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetNALBufferReady returned %d\n", __FUNCTION__, gvaReturnValue);)
        if (gvaReturnValue == APPLEGVA_RETURN_FAIL) {
            return MFX_ERR_ABORTED;
        }
        
        //Set the NALDescriptors
        gvaReturnValue = m_pSetNALDescriptors(m_gvaInformation.createAcceleratorParameter3, m_gvaIndex, 0, sliceCount, nalDescriptors);
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s SetNALDescriptors returned %d\n", __FUNCTION__, gvaReturnValue);)
        if (gvaReturnValue == APPLEGVA_RETURN_FAIL) {
            return MFX_ERR_ABORTED;
        }
        
        NALDescriptor * pUnknown;
        pUnknown = NULL;
        
        gvaReturnValue = m_pFunction15(m_gvaInformation.createAcceleratorParameter3, m_gvaIndex, 0, (NALDescriptor * *) &pUnknown);
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Function15 returned %d, pUnknown = %p\n", __FUNCTION__, gvaReturnValue, pUnknown);)
        if (gvaReturnValue == APPLEGVA_RETURN_FAIL) {
            return MFX_ERR_ABORTED;
        }
            
#if 0
        //Get decoder statistics
        decoderStatistics decodeStatistics;
        
        gvaReturnValue = m_pGetDecoderStatistics(m_gvaInformation.createAcceleratorParameter3, m_gvaIndex, 1, 3, (uint32_t *) &decodeStatistics);
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s GetDecoderStatistics returned %d\n", __FUNCTION__, gvaReturnValue);)
        if (gvaReturnValue == APPLEGVA_RETURN_FAIL) {
            return MFX_ERR_ABORTED;
        }
        else    {
            DEBUG_VTB_PRINT(printf("  VDATaskSupplier::%s frames received = %d, skipped = %d, degraded = %d, decoded = %d\n", __FUNCTION__, decodeStatistics.framesReceived, decodeStatistics.framesSkipped, decodeStatistics.framesDegraded, decodeStatistics.framesDecoded);)
        }
        
#endif  //if 0
            
        //Store the context for this frame
        frameCounter++;
        struct _decodeFrameContext thisFrameContext;
        thisFrameContext.frameNumber = frameCounter;
        thisFrameContext.frameType = frameType;
        thisFrameContext.pFrame = pFrame;
        
        m_callbackContextInformation.arrayFrameContexts[m_callbackIndex++] = thisFrameContext;
        
        if(m_callbackIndex >= maxNumberFrameContexts)  {
            
            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Resetting m_callbackContextInformation.arrayFrameContexts index\n", __FUNCTION__);)
            m_callbackIndex = 0;
        }
    }
    
    return UMC_OK;
}
#else   //USE_APPLEGVA
    
Status VDATaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    
    static unsigned int counter = 0;
    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Entry, counter = %d\n", __FUNCTION__, ++counter);)

    if(NULL == pFrame)  {
        return UMC_OK;
    }
   
    H264DecoderFrameInfo * sliceInfo = pFrame->GetAU(field);
    
    //check for slice groups, they are not supported by HD graphics
    if (sliceInfo->GetSlice(0)->IsSliceGroups())
    {
        DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s EXCEPTION slice groups are not supported\n", __FUNCTION__);)
        throw h264_exception(UMC_ERR_UNSUPPORTED);
    }
    
   if (sliceInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED) {
        return UMC_OK;
   }
   

    Status baseStatus;
    baseStatus = TaskSupplier::CompleteFrame(pFrame, field);
    if (UMC_OK != baseStatus) {
        return baseStatus;
    }
    
    OSStatus status;
    static Ipp32s frameCounter = 0;
    static Ipp8u mbaff = 255;
    const Ipp32s initalDeltaValue = 65535;
    static Ipp32s qp_delta = initalDeltaValue;
    static Ipp32s qs_delta = initalDeltaValue;
    
    Ipp32s frameType = pFrame->m_FrameType;
    Ipp32s frame_num = pFrame->m_FrameNum;
    
    char * pFrameTypeName;
    switch (frameType) {
        case 1:
            pFrameTypeName = "I Frame";
            break;
        case 2:
            pFrameTypeName = "P Frame";
            break;
        case 3:
            pFrameTypeName = "B Frame";
            break;
        default:
            pFrameTypeName = "Unknown Frame Type";
            break;
    }
    
    Ipp32s nalType;
    Ipp32s index = pFrame->m_index;
    CFMutableDictionaryRef frameInfo = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                 4,
                                                                 &kCFTypeDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks);
    CFNumberRef cfnrFrameCounter;
    CFNumberRef cfnrFrameType;
    CFNumberRef cfnrNalType;
    CFNumberRef cfnrIndex;
   
    DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s slice count = %d, field = %d, IsField = %d, IsBottom = %d\n", __FUNCTION__, sliceInfo->GetSliceCount(), field, sliceInfo->IsField(), sliceInfo->IsBottom());)

    unsigned int totalSizeEncodedBytes = 0;
    
    //Frame or Field?
    if(sliceInfo->IsField())  {
        
        Ipp8u *pNalUnit;    //ptr to first byte of start code
        Ipp32u NalUnitSize; // size of NAL unit in bytes
        
        char pFieldType[16];
        if (sliceInfo->IsBottom()) {
            strcpy(pFieldType, "bottom");
        }
        else    {
            strcpy(pFieldType, "top");
        }
            
        if(m_sizeField)    {
            
            //Combine previously cached field with this field and give it to the VTB decoder.
            //Make sure that the frames match. 
            if(m_pFieldFrame == pFrame)    {
                
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s recovered cached field of frame_num %d, size = %d\n", __FUNCTION__, frame_num, m_sizeField);)
                
                //Increment the byte count
                totalSizeEncodedBytes = m_sizeField;
                
                //Anything to prepend, such an additional/new PPS?
                if (!m_prependDataQueue.empty()) {
                    
                    struct nalStreamInfo nalInfo;
 
                    do {
                        
                        //Get the entry at the front
                        nalInfo = m_prependDataQueue.front();
                        m_prependDataQueue.pop();
                        
                        if (nalInfo.nalType != NAL_UT_SLICE) {
                            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s prepending data for frame_num %d, size = %d\n", __FUNCTION__, frame_num, nalInfo.headerBytes.size());)
                            for(unsigned int prependIndex = 0; prependIndex < nalInfo.headerBytes.size(); prependIndex++)    {
                                m_nals.push_back(nalInfo.headerBytes[prependIndex]);
                            }
                            totalSizeEncodedBytes += nalInfo.headerBytes.size();
                        }
                    } while (!m_prependDataQueue.empty() && (nalInfo.nalType != NAL_UT_SLICE));
                }
                
                //Put the second field into the nals vector
                for (Ipp32u i = 0; i < sliceInfo->GetSliceCount(); i++)
                {
                    H264Slice *pSlice = sliceInfo->GetSlice(i);
                    
                    pSlice->GetBitStream()->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
                    
                    //resize the vector if necessary
                    if (m_nals.size() < totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int)) {
                        m_nals.resize(totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int));
                    }
                    
                    //copy the nal data into the buffer
                    memcpy(&m_nals[totalSizeEncodedBytes+sizeof(unsigned int)], pNalUnit, NalUnitSize);
                    *((int * ) &m_nals[totalSizeEncodedBytes]) = htonl(NalUnitSize);
                    
                    //Increment the byte count
                    totalSizeEncodedBytes += NalUnitSize + sizeof(unsigned int);
                }
                nalType = m_nals[m_sizeField+sizeof(unsigned int)] & NAL_UNITTYPE_BITS;

                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s second field, %s (%d), NAL type %d, size = %d, size of both fields of frame_num %d = %d\n", __FUNCTION__, pFrameTypeName, frameType, nalType, totalSizeEncodedBytes - m_sizeField, frame_num, totalSizeEncodedBytes);)
                
                //Reset the top field information.  
                m_sizeField = 0;
                m_pFieldFrame = NULL;
            }
            else    {
                
                //Error, the frames are different.
                DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Failure, frames of two consecutive fields are different\n", __FUNCTION__);)
                return  MFX_ERR_ABORTED;
            }
        }
        else    {
            
            //This is the first field of this frame. 
            //Cache this field, do not give it to the VTB decoder now.
            m_nals.clear();
            totalSizeEncodedBytes = 0;
            
            //Anything to prepend, such an additional/new PPS?
            if (!m_prependDataQueue.empty()) {
                
                struct nalStreamInfo nalInfo;

                do {
                    
                    //Get the entry at the front
                    nalInfo = m_prependDataQueue.front();
                    m_prependDataQueue.pop();
                    
                    if (nalInfo.nalType != NAL_UT_SLICE) {
                        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s prepending data for frame_num %d, size = %d\n", __FUNCTION__, frame_num, nalInfo.headerBytes.size());)
                        for(unsigned int prependIndex = 0; prependIndex < nalInfo.headerBytes.size(); prependIndex++)    {
                            m_nals.push_back(nalInfo.headerBytes[prependIndex]);
                        }
                        totalSizeEncodedBytes += nalInfo.headerBytes.size();
                    }
                } while (!m_prependDataQueue.empty() && (nalInfo.nalType != NAL_UT_SLICE));
            }
            
            char pFieldType[16];
            
            if (sliceInfo->IsBottom()) {
                strcpy(pFieldType, "bottom");
            }
            else    {
                strcpy(pFieldType, "top");
            }
            
            for (Ipp32u i = 0; i < sliceInfo->GetSliceCount(); i++)
            {
                
                H264Slice *pSlice = sliceInfo->GetSlice(i);
                
                pSlice->GetBitStream()->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
                
                //resize the vector if necessary
                if (m_nals.size() < totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int)) {
                    m_nals.resize(totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int));
                }
                
                //copy the nal data into the buffer
                memcpy(&m_nals[totalSizeEncodedBytes+sizeof(unsigned int)], pNalUnit, NalUnitSize);
                *((int * ) &m_nals[totalSizeEncodedBytes]) = htonl(NalUnitSize);
                
                //Increment the byte count
                totalSizeEncodedBytes += NalUnitSize + sizeof(unsigned int);
            }
            m_sizeField = totalSizeEncodedBytes;
            m_pFieldFrame = pFrame;
            nalType = m_nals[sizeof(unsigned int)] & NAL_UNITTYPE_BITS;            
            
            DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s cached %s field of frame_num %d, %s (%d), NAL type %d, size = %d, %d slices\n", __FUNCTION__,  pFieldType, frame_num, pFrameTypeName, frameType, nalType, m_sizeField, sliceInfo->GetSliceCount());)

            nalType = m_nals[sizeof(unsigned int)] & NAL_UNITTYPE_BITS;

            return UMC_OK;
        }
    }
    else    {
        
        //It is a frame, not a field
        m_nals.clear();
        totalSizeEncodedBytes = 0;
        
        //Anything to prepend, such an additional/new PPS?
        if (!m_prependDataQueue.empty()) {
            
            struct nalStreamInfo nalInfo;

            do {
                
                //Get the entry at the front
                nalInfo = m_prependDataQueue.front();
                m_prependDataQueue.pop();
                
                if (nalInfo.nalType != NAL_UT_SLICE) {
                    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s prepending data for frame_num %d, size = %d\n", __FUNCTION__, frame_num, nalInfo.headerBytes.size());)
                    for(unsigned int prependIndex = 0; prependIndex < nalInfo.headerBytes.size(); prependIndex++)    {
                        m_nals.push_back(nalInfo.headerBytes[prependIndex]);
                    }
                    totalSizeEncodedBytes += nalInfo.headerBytes.size();
                }
            } while (!m_prependDataQueue.empty() && (nalInfo.nalType != NAL_UT_SLICE));
        }
        
        for (Ipp32u i = 0; i < sliceInfo->GetSliceCount(); i++)
        {
            H264Slice *pSlice = sliceInfo->GetSlice(i);

            
            if (pSlice->IsField()) {
                DEBUG_VTB_PRINT(printf("VDATaskSupplier::%s slice %d of frame %d IsField = %d, slice with field\n", __FUNCTION__, i, frame_num, sliceInfo->IsField());)
            }
            
            Ipp8u *pNalUnit;    //ptr to first byte of start code
            Ipp32u NalUnitSize; // size of NAL unit in bytes
            
            pSlice->GetBitStream()->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
            
            //resize the vector if necessary
            if (m_nals.size() < totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int)) {
                m_nals.resize(totalSizeEncodedBytes + NalUnitSize + sizeof(unsigned int));
            }
            
            //copy the nal data into the buffer
            memcpy(&m_nals[totalSizeEncodedBytes+sizeof(unsigned int)], pNalUnit, NalUnitSize);
            *((int * ) &m_nals[totalSizeEncodedBytes]) = htonl(NalUnitSize);
                             
            //Increment the byte count
            totalSizeEncodedBytes += NalUnitSize + sizeof(unsigned int);
        }
    }
    
    //Decode the frame
    //Setup the context information for this NAL
    frameCounter++;
    cfnrFrameCounter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameCounter);
    CFDictionarySetValue(frameInfo, CFSTR("vdaFrameCounter"), cfnrFrameCounter);
    
    cfnrFrameType = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameType);
    CFDictionarySetValue(frameInfo, CFSTR("vdaFrameType"), cfnrFrameType);
    
    nalType = m_nals[sizeof(unsigned int)] & NAL_UNITTYPE_BITS;
    cfnrNalType = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &nalType);
    CFDictionarySetValue(frameInfo, CFSTR("vdaNalType"), cfnrNalType);
    
    cfnrIndex = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &index);
    CFDictionarySetValue(frameInfo, CFSTR("vdaIndex"), cfnrIndex);
    
    //Create the block buffer used in the sample buffer
    CMBlockBufferRef nalBuffer;
    status = CMBlockBufferCreateWithMemoryBlock (kCFAllocatorDefault,
                                                 &m_nals[0],
                                                 totalSizeEncodedBytes,
                                                 kCFAllocatorNull,
                                                 NULL,
                                                 0,
                                                 totalSizeEncodedBytes,
                                                 0,
                                                 &nalBuffer);
    
    if(noErr != status)
    {
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s CMBlockBufferCreateWithMemoryBlock failed, return code = %d\n", __FUNCTION__, status);)
        return  MFX_ERR_ABORTED;            
    }
    
    //Create the sample buffer
    CMSampleBufferRef nalSampleBuffer;
    
    //Sample size information, because QTP does it
    size_t sampleSize = totalSizeEncodedBytes;
    
    static CMTime decodeTimestamp, sampleDuration;
    static bool bDTSInitialized = false;
    if(bDTSInitialized == false)    {
        
        decodeTimestamp = CMTimeMake ((int64_t) 0, 24);
        decodeTimestamp.flags = kCMTimeFlags_Valid;
        decodeTimestamp.epoch = 0;
        sampleDuration = CMTimeMake ((int64_t) 1, 24);
        sampleDuration.flags = kCMTimeFlags_Valid;
        sampleDuration.epoch = 0;
        bDTSInitialized = true;
    }
    else    {
        
        //Increment the decode timestamp
        decodeTimestamp.value++;
    }
    
    CMTime presentationTimestamp = CMTimeMake ((int64_t) pFrame->m_PicOrderCnt[0], 24);
    presentationTimestamp.flags = kCMTimeFlags_Valid;
    presentationTimestamp.epoch = 0;
    
    if(decodeTimestamp.value != presentationTimestamp.value)    {
        
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s Attention: decodeTimestamp.value = %d, presentationTimestamp.value = %d\n", __FUNCTION__, decodeTimestamp.value, presentationTimestamp.value);)
    }
    
    CMSampleTimingInfo sampleTimingInformation;
    sampleTimingInformation.duration = sampleDuration;
    sampleTimingInformation.presentationTimeStamp = presentationTimestamp;
    sampleTimingInformation.decodeTimeStamp = decodeTimestamp;
    
    //Create the sample buffer
    status = CMSampleBufferCreate (kCFAllocatorDefault,
                                   nalBuffer,
                                   TRUE,
                                   0,    //NULL,
                                   0,    //NULL,
                                   m_sourceFormatDescription,
                                   1,
                                   1,    //one sample timing entry
                                   &sampleTimingInformation,
                                   1,    //one sample size information
                                   &sampleSize,
                                   &nalSampleBuffer);
    
    if(noErr != status)
    {
        CFRelease(nalBuffer);
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s CMSampleBufferCreate failed, return code = %d\n", __FUNCTION__, status);)
        return  MFX_ERR_ABORTED;
    }
    
//    CMSampleBufferSetOutputPresentationTimeStamp(nalSampleBuffer, presentationTimestamp);
    
    if(frameCounter == 1)
    {
        int outputValue = 1;
        CFNumberRef resumeOutputValue;
        resumeOutputValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &outputValue);
        CMSetAttachment(nalSampleBuffer, CFSTR("ResumeOutput"), resumeOutputValue, kCMAttachmentMode_ShouldPropagate);
        CMSetAttachment(nalSampleBuffer, CFSTR("ResetDecoderBeforeDecoding"), kCFBooleanTrue, kCMAttachmentMode_ShouldPropagate);
    }
    
    VTDecodeInfoFlags decodeFlagsOut;
    
    VTDecodeFrameFlags decodeFlags;
    
    decodeFlags = kVTDecodeFrame_EnableTemporalProcessing | kVTDecodeFrame_EnableAsynchronousDecompression;
    if(frameCounter == 2)   {
        decodeFlags |= kVTDecodeFrame_1xRealTimePlayback;
    }
    
    //kVTDecodeFrame_1xRealTimePlayback
    status = CALL_VTB_FUNC(VTDecompressionSessionDecodeFrame)(m_decompressionSession,
                                               nalSampleBuffer,
                                               decodeFlags,
                                               frameInfo,
                                               &decodeFlagsOut);

    CFRelease(nalBuffer);
    CFRelease(nalSampleBuffer);

    if(noErr != status)
    {
        DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s VTDecompressionSessionDecodeFrame failed, return code = %s (%d)\n", __FUNCTION__, getVTErrorString(status), status);)
        return  MFX_ERR_ABORTED;
    }
    DEBUG_VTB_PRINT(printf(" VDATaskSupplier::%s delivered frame %d to VTB decoder, frame_num = %d, %s (%d), NAL type %d, flags = %d, size = %d, DTS = %ld, PTS = %ld, duration = %ld\n", __FUNCTION__, frameCounter, frame_num, pFrameTypeName, frameType, nalType, decodeFlagsOut, totalSizeEncodedBytes, sampleTimingInformation.decodeTimeStamp.value, sampleTimingInformation.presentationTimeStamp.value, sampleTimingInformation.duration.value);)
    CALL_VTB_FUNC(VTDecompressionSessionWaitForAsynchronousFrames)(m_decompressionSession);
        
        
#ifdef TRY_PROPERMEMORYMANAGEMENT   
   if (frameInfo) {
      CFRelease(frameInfo);
   }
#endif
   
    return UMC_OK;
}
#endif   //USE_APPLEGVA

inline bool isFreeFrame(H264DecoderFrame * pTmp)
{ 
    return (!pTmp->m_isShortTermRef[0] &&
        !pTmp->m_isShortTermRef[1] &&
        !pTmp->m_isLongTermRef[0] &&
        !pTmp->m_isLongTermRef[1] //&&
        );
}

Status VDATaskSupplier::AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat chroma_format_idc)
{    

    VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, chroma_format_idc, bit_depth);
    
    FrameMemID frmMID;
    Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);
    
    if (sts != UMC_OK)
    {
        throw h264_exception(UMC_ERR_ALLOC);
    }
        
    const FrameData *frmData = m_pFrameAllocator->Lock(frmMID);
    
    if (!frmData)
        throw h264_exception(UMC_ERR_LOCK);
    
    pFrame->allocate(frmData, &info);
    
    Status umcRes = pFrame->allocateParsedFrameData();
    
    //Added this one line from the skeleton code below
    pFrame->m_index = frmMID;

    return umcRes;     
}

H264Slice * VDATaskSupplier::DecodeSliceHeader(MediaDataEx *nalUnit)
{
    size_t dataSize = nalUnit->GetDataSize();
    nalUnit->SetDataSize(IPP_MIN(1024, dataSize));

    H264Slice * slice = TaskSupplier::DecodeSliceHeader(nalUnit);

    nalUnit->SetDataSize(dataSize);

    if (!slice)
        return 0;

    H264MemoryPiece * pMemCopy = m_Heap.Allocate(nalUnit->GetDataSize() + 8);
    notifier1<H264_Heap, H264MemoryPiece*> memory_leak_preventing1(&m_Heap, &H264_Heap::Free, pMemCopy);

    memcpy(pMemCopy->GetPointer(), nalUnit->GetDataPointer(), nalUnit->GetDataSize());
    memset(pMemCopy->GetPointer() + nalUnit->GetDataSize(), 0, 8);
    pMemCopy->SetDataSize(nalUnit->GetDataSize());
    pMemCopy->SetTime(nalUnit->GetTime());

    Ipp32u* pbs;
    Ipp32u bitOffset;

    slice->m_BitStream.GetState(&pbs, &bitOffset);

    size_t bytes = slice->m_BitStream.BytesDecodedRoundOff();

    m_Heap.Free(slice->m_pSource);

    slice->m_pSource = pMemCopy;
    slice->m_BitStream.Reset(slice->m_pSource->GetPointer(), bitOffset,
        (Ipp32u)slice->m_pSource->GetDataSize());
    slice->m_BitStream.SetState((Ipp32u*)(slice->m_pSource->GetPointer() + bytes), bitOffset);

    memory_leak_preventing1.ClearNotification();

    return slice;
}

// *******************************************************************
// ***  TaskBrokerSingleThreadVDA methods
// *******************************************************************
TaskBrokerSingleThreadVDA::TaskBrokerSingleThreadVDA(TaskSupplier * pTaskSupplier)
    : TaskBrokerSingleThread(pTaskSupplier)
    , m_lastCounter(0)
{
    m_counterFrequency = vm_time_get_frequency();
}

void TaskBrokerSingleThreadVDA::WaitFrameCompletion()
{
}

bool TaskBrokerSingleThreadVDA::PrepareFrame(H264DecoderFrame * pFrame)
{
    if (!pFrame || pFrame->m_iResourceNumber < 0)
    {
        return true;
    }

    bool isSliceGroups = pFrame->GetAU(0)->IsSliceGroups() || pFrame->GetAU(1)->IsSliceGroups();
    if (isSliceGroups)
        pFrame->m_iResourceNumber = 0;

    if (pFrame->prepared[0] && pFrame->prepared[1])
        return true;

    if (!pFrame->prepared[0] &&
        (pFrame->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared[0] = true;
    }

    if (!pFrame->prepared[1] &&
        (pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared[1] = true;
    }

    return true;
}

void TaskBrokerSingleThreadVDA::Reset()
{
    m_lastCounter = 0;
    TaskBrokerSingleThread::Reset();
}

void TaskBrokerSingleThreadVDA::Start()
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::Start();
    m_completedQueue.clear();

}

void TaskBrokerSingleThreadVDA::decoderOutputCallback(CFDictionaryRef frameInfo, OSStatus status, uint32_t infoFlags, CVImageBufferRef imageBuffer)
{
    
    //Get context information
    CFNumberRef frameType = NULL;
    int32_t frameTypeValue = -1;
    CFNumberRef frameCounter = NULL;
    int32_t frameCounterValue = -1;
    CFNumberRef nalType = NULL;
    int32_t nalTypeValue = -1;
    CFNumberRef index = NULL;
    int32_t indexValue = -1;
    if (NULL != frameInfo) {
        frameType = (CFNumberRef) CFDictionaryGetValue(frameInfo, CFSTR("vdaFrameType"));
        if(frameType)    {
            CFNumberGetValue(frameType, kCFNumberSInt32Type, &frameTypeValue);
            CFRelease(frameType);
        }
        frameCounter = (CFNumberRef) CFDictionaryGetValue(frameInfo, CFSTR("vdaFrameCounter"));
        if(frameCounter)    {
            CFNumberGetValue(frameCounter, kCFNumberSInt32Type, &frameCounterValue);
            CFRelease(frameCounter);
        }
        nalType = (CFNumberRef) CFDictionaryGetValue(frameInfo, CFSTR("vdaNalType"));
        if(nalType)    {
            CFNumberGetValue(nalType, kCFNumberSInt32Type, &nalTypeValue);
            CFRelease(nalType);
        }
        index = (CFNumberRef) CFDictionaryGetValue(frameInfo, CFSTR("vdaIndex"));
        if(index)    {
            CFNumberGetValue(index, kCFNumberSInt32Type, &indexValue);
            CFRelease(index);
        }
        //Can't remove becuase frameInfo is actually a const CFDictionary *      CFDictionaryRemoveAllValues(frameInfo);
//        CFRelease(frameInfo); //Releasing here caused seg fault in 6.6.1.8 (and perhaps others)
    }
    else {
        DEBUG_VTB_PRINT(printf("TaskBrokerSingleThreadVDA::%s called with NULL pointer to context information!", __FUNCTION__);)
        return;
    }
    
    if (NULL == imageBuffer) {
        DEBUG_VTB_PRINT(printf(" TaskBrokerSingleThreadVDA::%s NULL image buffer! status = %d (%s), flags = %#x, frameType = %d, frameCounter = %d, nalType = %d, imageBuffer %p\n", __FUNCTION__, status, getVTErrorString(status), infoFlags, frameTypeValue, frameCounterValue, nalTypeValue, imageBuffer);)
        
        return;
    }
    else    {

        DEBUG_VTB_PRINT(printf(" TaskBrokerSingleThreadVDA::%s status = %d, flags = %#x, frameType = %d, frameCounter = %d, nalType = %d, imageBuffer %p\n", __FUNCTION__, status, infoFlags, frameTypeValue, frameCounterValue, nalTypeValue, imageBuffer);)
        
    }
        
    ViewItem &view = m_pTaskSupplier->GetViewByNumber(BASE_VIEW);
    Ipp32s viewCount = m_pTaskSupplier->GetViewCount();
    
    H264DecoderFrame *pFrame = view.GetDPBList(0)->head();
    bool bFoundFrame = false;
    for (; pFrame && !bFoundFrame; pFrame = pFrame->future())
    {
        if(indexValue == pFrame->m_index)  {
            bFoundFrame = true;
            break;
        }
    }
    if (!bFoundFrame) {
        
        DEBUG_VTB_PRINT(printf(" TaskBrokerSingleThreadVDA::%s Did not find the frame with m_index = %d in the DPB\n", __FUNCTION__, indexValue);)
    }
    
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    OSType pixelFormatType = CVPixelBufferGetPixelFormatType(imageBuffer);
        
    if((noErr == status) && bFoundFrame)
    {
        //Decoding was successful
        
        void * pPixelBaseAddress;
        mfxU8 * pPixels, * pPixelsU, * pPixelsV;
        mfxU8 * pBaseAddressY = NULL;
        mfxU8 * pBaseAddressU = NULL;
        mfxU8 * pBaseAddressV = NULL;
        mfxU8 * pDecodedY = NULL;
        mfxU8 * pDecodedU = NULL;
        mfxU8 * pDecodedV = NULL;
        size_t bufferHeight, bufferWidth, bytesPerRow, planeCount;
        size_t bufferHeightY, bufferWidthY, bytesPerRowY;
        size_t bufferHeightU, bufferWidthU, bytesPerRowU;
        size_t bufferHeightV, bufferWidthV, bytesPerRowV;
        int row, column;
        int uvIndex, yIndex;
        
        // copy data from VDA_frame to out_y, out_u, out_v
        CVPixelBufferLockBaseAddress(imageBuffer, 0);
        planeCount = CVPixelBufferGetPlaneCount(imageBuffer);
        pPixelBaseAddress = CVPixelBufferGetBaseAddress(imageBuffer);
        
        //Check pixel format
        //Is the pixel format 420?
        if(pixelFormatType == kCVPixelFormatType_420YpCbCr8Planar)  {
        
            pBaseAddressY = (mfxU8 *) CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
            bytesPerRowY = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
            bufferHeightY = CVPixelBufferGetHeightOfPlane(imageBuffer, 0);
            bufferWidthY = CVPixelBufferGetWidthOfPlane(imageBuffer, 0);
            if (planeCount > 0) {
                pBaseAddressU = (mfxU8 *) CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
                bytesPerRowU = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);
                bufferHeightU = CVPixelBufferGetHeightOfPlane(imageBuffer, 1);
                bufferWidthU = CVPixelBufferGetWidthOfPlane(imageBuffer, 1);
            }
            if (planeCount > 1) {
                pBaseAddressV = (mfxU8 *) CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 2);
                bytesPerRowV = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 2);
                bufferHeightV = CVPixelBufferGetHeightOfPlane(imageBuffer, 2);
                bufferWidthV = CVPixelBufferGetWidthOfPlane(imageBuffer, 2);
            }
            
            pDecodedY = pBaseAddressY;
            pDecodedU = pBaseAddressU;
            pDecodedV = pBaseAddressV;
            
            //Any extended pixels?  (cropping)
            size_t extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom;
            CVPixelBufferGetExtendedPixels (imageBuffer, &extraColumnsLeft, &extraColumnsRight, &extraRowsTop, &extraRowsBottom);
            if((extraColumnsLeft != 0) || (extraColumnsRight != 0) || (extraRowsTop != 0) || (extraRowsBottom != 0))    {
                DEBUG_VTB_PRINT(printf(" Extended Pixel Information: left = %d, right = %d, top = %d, bottom = %d\n", extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom);)
            }
            
            //Copy Y
            yIndex = 0;
            bufferHeight = bufferHeightY;
            bufferWidth = bufferWidthY;
            bytesPerRow = bytesPerRowY;
            pPixels = pDecodedY;

#ifdef PRINT_HISTOGRAM
            mfxU8 yHistogram[256], uHistogram[256], vHistogram[256];
            for(unsigned int index = 0; index < 256; index++)   {
                yHistogram[index] = 0;
                uHistogram[index] = 0;
                vHistogram[index] = 0;
            }
#endif
        
            mfxU8 * pDestination = pFrame->m_pYPlane;
            DEBUG_VTB_PRINT(printf(" 420YpCbCr8Planar Y: height = %d, width = %d, bytes per row = %d, output pitch = %d, plane count = %d\n", bufferHeight, bufferWidth, bytesPerRow, pFrame->pitch_luma(), planeCount);)
            
            for(row = 0; row < bufferHeight; row++)   {
                
#ifdef USE_MEMCPY
                memcpy(pDestination, pPixels, bufferWidth);
#else
                for(column = 0, yIndex = 0; column < bufferWidth; column++, yIndex++)   {
                    pDestination[yIndex] = (pPixels)[column];
#ifdef PRINT_HISTOGRAM
                    yHistogram[pDestination[yIndex]]++;
#endif
                }
#endif
                pDestination += pFrame->pitch_luma();
                pPixels += bytesPerRow;
            }
            
#ifdef PRINT_HISTOGRAM            
            DEBUG_VTB_PRINT(printf(" Y histogram frame %d", frameCounterValue);)
            for (unsigned int i = 0; i < 256; i += 16) {
                DEBUG_VTB_PRINT(printf("\n %3d:", i);)
                for (unsigned int j = 0; j < 16 && i+j < 256; j++) {
                    DEBUG_VTB_PRINT(printf(" %4d", yHistogram[i+j]);)
                }
            }
            DEBUG_VTB_PRINT(printf("\n");)
#endif
            //Copy U and V
            uvIndex = 0;
            bufferHeight = bufferHeightU;
            bufferWidth = bufferWidthU;
            bytesPerRow = bytesPerRowU;
            pPixelsU = pDecodedU;
            pPixelsV = pDecodedV;
            if((bytesPerRowU != bytesPerRowV) || (bufferHeightU != bufferHeightV) || (bufferWidthU != bufferWidthV))
            {
                DEBUG_VTB_PRINT(printf(" TaskBrokerSingleThreadVDA::%s WARNING chroma buffer parameters do not match ********\n", __FUNCTION__);)
                DEBUG_VTB_PRINT(printf("  buffer row x height: U = %dx%d, V = %dx%d; bytes per row: U = %d, V = %d\n", bufferWidthU, bufferHeightU, bufferWidthV, bufferHeightV, bytesPerRowU, bytesPerRowV);)
            }
            
            pDestination = pFrame->m_pUVPlane;
            DEBUG_VTB_PRINT(printf(" 420YpCbCr8Planar UV: height = %d, width = %d, bytes per row = %d, output pitch = %d\n", bufferHeight, bufferWidth, bytesPerRow, pFrame->pitch_chroma());)
            for(row = 0; row < bufferHeight; row++)   {
                for(column = 0, uvIndex = 0; column < bufferWidth; column++, uvIndex += 2)   {
                    pDestination[uvIndex] = (pPixelsU)[column];
                    pDestination[uvIndex+1] = (pPixelsV)[column];
#ifdef PRINT_HISTOGRAM
                    uHistogram[pDestination[uvIndex]]++;
                    vHistogram[pDestination[uvIndex+1]]++;
#endif
                }
                pPixelsU += bytesPerRow;
                pPixelsV += bytesPerRow;
                pDestination += pFrame->pitch_chroma();
            }
            
#ifdef PRINT_HISTOGRAM            
            DEBUG_VTB_PRINT(printf(" U histogram frame %d", frameCounterValue);)
            for (unsigned int i = 0; i < 256; i += 16) {
                DEBUG_VTB_PRINT(printf("\n %3d:", i);)
                for (unsigned int j = 0; j < 16 && i+j < 256; j++) {
                    DEBUG_VTB_PRINT(printf(" %4d", uHistogram[i+j]);)
                }
            }
            DEBUG_VTB_PRINT(printf("\n");)
            DEBUG_VTB_PRINT(printf(" V histogram frame %d", frameCounterValue);)
            for (unsigned int i = 0; i < 256; i += 16) {
                DEBUG_VTB_PRINT(printf("\n %3d:", i);)
                for (unsigned int j = 0; j < 16 && i+j < 256; j++) {
                    DEBUG_VTB_PRINT(printf(" %4d", vHistogram[i+j]);)
                }
            }
            DEBUG_VTB_PRINT(printf("\n");)
#endif
        }
        
        else if(pixelFormatType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)  {
            
            pBaseAddressY = (mfxU8 *) CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
            bytesPerRowY = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
            bufferHeightY = CVPixelBufferGetHeightOfPlane(imageBuffer, 0);
            bufferWidthY = CVPixelBufferGetWidthOfPlane(imageBuffer, 0);
            if (planeCount > 0) {
                pBaseAddressU = (mfxU8 *) CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
                bytesPerRowU = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);
                bufferHeightU = CVPixelBufferGetHeightOfPlane(imageBuffer, 1);
                bufferWidthU = CVPixelBufferGetWidthOfPlane(imageBuffer, 1);
            }
            
            pDecodedY = pBaseAddressY;
            pDecodedU = pBaseAddressU;
            
            //Any extended pixels?  (cropping)
            size_t extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom;
            CVPixelBufferGetExtendedPixels (imageBuffer, &extraColumnsLeft, &extraColumnsRight, &extraRowsTop, &extraRowsBottom);
            if((extraColumnsLeft != 0) || (extraColumnsRight != 0) || (extraRowsTop != 0) || (extraRowsBottom != 0))    {
                DEBUG_VTB_PRINT(printf(" Extended Pixel Information: left = %d, right = %d, top = %d, bottom = %d\n", extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom);)
            }
            
            //Copy Y
            yIndex = 0;
            bufferHeight = bufferHeightY;
            bufferWidth = bufferWidthY;
            bytesPerRow = bytesPerRowY;
            pPixels = pDecodedY;
            
            mfxU8 * pDestination = pFrame->m_pYPlane;
            DEBUG_VTB_PRINT(printf(" 420YpCbCr8BiPlanarVideoRange Y: height = %d, width = %d, bytes per row = %d, output pitch = %d, plane count = %d\n", bufferHeight, bufferWidth, bytesPerRow, pFrame->pitch_luma(), planeCount);)
            IppiSize regionOfInterest;
            regionOfInterest.width = bufferWidth;
            regionOfInterest.height = bufferHeight;
            ippiCopyManaged_8u_C1R(pPixels, bytesPerRow, pDestination, pFrame->pitch_luma(), regionOfInterest, 2);
            
/*            for(row = 0; row < bufferHeight; row++)   {
#ifdef USE_MEMCPY
                memcpy(pDestination, pPixels, bufferWidth);
#else
                for(column = 0, yIndex = 0; column < bufferWidth; column++, yIndex++)   {
                    pDestination[yIndex] = (pPixels)[column];
                }
#endif
                pDestination += pFrame->pitch_luma();
                pPixels += bytesPerRow;
            }*/
            
            //Copy U and V
            uvIndex = 0;
            bufferHeight = bufferHeightU;
            bufferWidth = bufferWidthU;
            bytesPerRow = bytesPerRowU;
            pPixelsU = pDecodedU;
            
            pDestination = pFrame->m_pUVPlane;
            DEBUG_VTB_PRINT(printf(" 420YpCbCr8BiPlanarVideoRange UV: height = %d, width = %d, bytes per row = %d, output pitch = %d\n", bufferHeight, bufferWidth, bytesPerRow, pFrame->pitch_chroma());)
            
            regionOfInterest.width = bufferWidth;
            regionOfInterest.height = bufferHeight;
            ippiCopyManaged_8u_C1R(pPixelsU, bytesPerRow, pDestination, pFrame->pitch_chroma(), regionOfInterest, 2);
            
            for(row = 0; row < bufferHeight; row++)   {
#ifdef USE_MEMCPY
                memcpy(pDestination, pPixelsU, 2*bufferWidth);
#else
                for(column = 0, uvIndex = 0; column < bufferWidth; column++, uvIndex += 2)   {
                    pDestination[uvIndex] = (pPixelsU)[uvIndex];
                    pDestination[uvIndex+1] = (pPixelsU)[uvIndex+1];
                }
#endif
                pPixelsU += bytesPerRow;
                pDestination += pFrame->pitch_chroma();
            }
        }
        //Is the pixel format 422?
        else if(pixelFormatType == kCVPixelFormatType_422YpCbCr8)   {
            //kCVPixelFormatType_422YpCbCr8 = '2vuy',     /* Component Y'CbCr 8-bit 4:2:2, ordered Cb Y'0 Cr Y'1 */
            
            size_t extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom;
            CVPixelBufferGetExtendedPixels (imageBuffer, &extraColumnsLeft, &extraColumnsRight, &extraRowsTop, &extraRowsBottom);
            if((extraColumnsLeft != 0) || (extraColumnsRight != 0) || (extraRowsTop != 0) || (extraRowsBottom != 0))    {
                DEBUG_VTB_PRINT(printf(" Extended Pixel Information: left = %d, right = %d, top = %d, bottom = %d\n", extraColumnsLeft, extraColumnsRight, extraRowsTop, extraRowsBottom);)
            }
            
            //Only expect one plane for 422
            if (planeCount > 0) {
                //DEBUG_PRINT((" TaskBrokerSingleThreadVDA::%s WARNING plane count = % for kCVPixelFormatType_422YpCbCr8 pixel format ********\n", __FUNCTION__, planeCount));
            }
            
            //Prepare to copy the decoded pixels
            size_t bufferHeight, bufferWidth, bytesPerRow;
            mfxU8 * pDecodedPixels = NULL;
            mfxU8 * pYDestination = pFrame->m_pYPlane;
            mfxU8 * pUVDestination = pFrame->m_pUVPlane;
            
            pDecodedPixels = (mfxU8 *) CVPixelBufferGetBaseAddress(imageBuffer);
            bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);
            bufferHeight = CVPixelBufferGetHeight(imageBuffer);
            bufferWidth = CVPixelBufferGetWidth(imageBuffer);
            
            DEBUG_VTB_PRINT(printf(" pixel format 422YpCbCr8: height = %d, width = %d, bytes per row = %d, output pitch = %d, plane count = %d\n", bufferHeight, bufferWidth, bytesPerRow, pFrame->pitch_luma(), planeCount);)
            
            //Copy the decoded pixels to the MSDK buffers
            //NOTE: There is a chroma bug here in the output because MSDK wants 420. A conversion is necessary.
            int row, column;
            int destinationIndex;
            int inputIndex;
            for(row = 0; row < bufferHeight; row++)   {
                for(column = 0, destinationIndex = 0, inputIndex = 0; column < bufferWidth; column += 2, inputIndex += 4, destinationIndex += 2)   {
                    
                    /* Component Y'CbCr 8-bit 4:2:2, ordered Cb Y'0 Cr Y'1 */
                    pUVDestination[destinationIndex] = pDecodedPixels[inputIndex];        //copy Cb
                    pYDestination[destinationIndex] = pDecodedPixels[inputIndex+1];       //copy Y'0
                    pUVDestination[destinationIndex+1] = pDecodedPixels[inputIndex+2];    //copy Cr
                    pYDestination[destinationIndex+1] = pDecodedPixels[inputIndex+3];     //copy Y'1
                }
                pYDestination += pFrame->pitch_luma();
                pUVDestination += pFrame->pitch_chroma();
                pDecodedPixels += bytesPerRow;
            }
            
        }
        else    {
            
            //This pixel format type  is not supported (yet).
            DEBUG_VTB_PRINT(printf(" TaskBrokerSingleThreadVDA::%s ERROR pixel format %d is not supported ********\n", __FUNCTION__, pixelFormatType);)
            CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
            return;
        }
        
        CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
//Caused segmentation fault        CFRelease(imageBuffer);
        
        AutomaticUMCMutex guard(m_mGuard);
        
        //Field or frame?
        if (pFrame->m_PictureStructureForDec < FRM_STRUCTURE)
        {
            
            //Field
            DEBUG_VTB_PRINT(printf("TaskBrokerSingleThreadVDA::%s following interlaced path\n", __FUNCTION__);)
            ReportItem reportItemTopField(pFrame->m_index, 0, ERROR_FRAME_NONE);
            m_reports.push_back(reportItemTopField);
            ReportItem reportItemBottomField(pFrame->m_index, 1, ERROR_FRAME_NONE);
            m_reports.push_back(reportItemBottomField);
        }
        else
        {
            
            //Frame
            DEBUG_VTB_PRINT(printf("TaskBrokerSingleThreadVDA::%s following progressive path\n", __FUNCTION__);)
            ReportItem reportItem(pFrame->m_index, 0, ERROR_FRAME_NONE);
            m_reports.push_back(reportItem);
        }
    }
}
    
void TaskBrokerSingleThreadVDA::AddReportItem(Ipp32u index, Ipp32u field, Ipp8u status)
{
    AutomaticUMCMutex guard(m_mGuard);

    ReportItem newReportItem(index, field, status);
    m_reports.push_back(newReportItem);
}

void TaskBrokerSingleThreadVDA::decoderOutputCallback2(void *decompressionOutputRefCon,
                                       void * sourceFrameRefCon,
                                       OSStatus status,
                                       VTDecodeInfoFlags infoFlags,
                                       CVImageBufferRef imageBuffer,
                                       CMTime presentationTimeStamp,
                                       CMTime presentationDuration)
{
#if 1//def TRY_PROPERMEMORYMANAGEMENT
        if (imageBuffer)
        {
            CFRetain(imageBuffer);
        }
#endif
        DEBUG_VTB_PRINT(printf("TaskBrokerSingleThreadVDA::%s presentation time stamp = %ld\n", __FUNCTION__, presentationTimeStamp.value);)
        TaskBrokerSingleThreadVDA * pTaskBrokerSingleThreadVDA = (TaskBrokerSingleThreadVDA *) decompressionOutputRefCon;
        pTaskBrokerSingleThreadVDA->decoderOutputCallback((CFDictionaryRef) sourceFrameRefCon, status, infoFlags, imageBuffer);
#if 1   //def TRY_PROPERMEMORYMANAGEMENT
        if (imageBuffer)
        {
            CFRelease(imageBuffer);
        }
#endif    
}
    
bool TaskBrokerSingleThreadVDA::GetNextTaskInternal(H264Task *)
{
    DEBUG_VTB_PRINT(printf("TaskBrokerSingleThreadVDA::%s Entry\n", __FUNCTION__));
    bool wasCompleted = false;

    if (m_reports.size() && m_FirstAU)
    {
        Ipp32u i = 0;
        H264DecoderFrameInfo * au = m_FirstAU;
        bool wasFound = false;
        while (au)
        {
            for (; i < m_reports.size(); i++)
            {
                // find Frame
                if ((m_reports[i].m_index == (Ipp32u)au->m_pFrame->m_index) && (au->IsBottom() == (m_reports[i].m_field != 0)))
                {

                    switch (m_reports[i].m_status)
                    {
                    case 1:
                        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MINOR);
                        break;
                    case 2:
                        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
                        break;
                    case 3:
                        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
                        break;
                    case 4:
                        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
                        break;
                    }

                    //Mark frame as completed
                    au->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
                    CompleteFrame(au->m_pFrame);
                    wasFound = true;
                    wasCompleted = true;
                    
                    m_reports.erase(m_reports.begin() + i);//, (i + 1 == m_reports.size()) ? m_reports.end() : m_reports.begin() + i + i);
                    break;
                }
            }

            if (!wasFound)
                break;

            au = au->GetNextAU();
        }

        SwitchCurrentAU();
    }

    if (!wasCompleted && m_FirstAU)
    {
        Ipp64u currentCounter = (Ipp64u) vm_time_get_tick();

        if (m_lastCounter == 0)
            m_lastCounter = currentCounter;

        Ipp64u diff = (currentCounter - m_lastCounter);
        if (diff >= m_counterFrequency)
        {
                        
            Report::iterator iter = std::find(m_reports.begin(), m_reports.end(), ReportItem(m_FirstAU->m_pFrame->m_index, m_FirstAU->IsBottom(), 0));
            if (iter != m_reports.end())
            {
                m_reports.erase(iter);
            }

            m_FirstAU->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
            m_FirstAU->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            CompleteFrame(m_FirstAU->m_pFrame);

            SwitchCurrentAU();
            m_lastCounter = 0;
        }
    }
    else
    {
        m_lastCounter = 0;
    }

    return false;
}

void TaskBrokerSingleThreadVDA::AwakeThreads()
{
}

// *******************************************************************
// ***  H264_VDA_SegmentDecoder methods
// *******************************************************************    
H264_VDA_SegmentDecoder::H264_VDA_SegmentDecoder(TaskSupplier * pTaskSupplier)
    : H264SegmentDecoderMultiThreaded(pTaskSupplier->GetTaskBroker())
{
}

Status H264_VDA_SegmentDecoder::ProcessSegment(void)
{
    H264Task Task(m_iNumber);

    if (!m_pTaskBroker->GetNextTask(&Task))
    {

        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    return UMC_OK;
}

} // namespace UMC

#endif // UMC_VA_OSX
#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H264_VIDEO_DECODER
#endif // __APPLE__
