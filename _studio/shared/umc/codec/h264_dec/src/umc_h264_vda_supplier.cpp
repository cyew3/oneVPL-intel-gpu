/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#ifdef __APPLE__

//#define TRY_PROPERMEMORYMANAGEMENT

#include "umc_defs.h"

#include <VDADecoder.h> //For Apple VideoDecodeAcceleration Framework

#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef UMC_RESTRICTED_CODE_VA

#include <algorithm>

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

#ifndef ERROR_FRAME_NONE
#define ERROR_FRAME_NONE 0
#endif

#ifndef ERROR_FRAME_MINOR
#define ERROR_FRAME_MINOR 1
#endif

#ifndef ERROR_FRAME_MAJOR
#define ERROR_FRAME_MAJOR 1
#endif

namespace UMC
{

//VDATaskSupplier Definitions
VDATaskSupplier::VDATaskSupplier()
    : m_bufferedFrameNumber(0)
{
    m_isHaveSPS = false;
    m_isHavePPS = false;
    m_isVDAInstantiated = false;
}
    
VDATaskSupplier::~VDATaskSupplier()
{
    m_isHaveSPS = false;
    m_isHavePPS = false;
    if (true == m_isVDAInstantiated) {
        VDADecoderFlush(m_VDADecoder, 0);
        VDADecoderDestroy(m_VDADecoder);  
    }
    m_isVDAInstantiated = false;
}    

Status VDATaskSupplier::Init(BaseCodecParams *pInit)
{
    
    m_pMemoryAllocator = pInit->lpMemoryAllocator;

    Status umsRes = TaskSupplier::Init(pInit);
    if (umsRes != UMC_OK)
        return umsRes;

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
    m_pTaskBroker->Init(m_iThreadNum, true);

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264_VDA_SegmentDecoder(this);
        if (0 == m_pSegmentDecoder[i])
            return UMC_ERR_ALLOC;
    }

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC_ERR_INIT;
    }

    m_DPBSizeEx = 1;

    m_sei_messages = new SEI_Storer();
    m_sei_messages->Init();

    return UMC_OK;
}
    
void VDATaskSupplier::Close()
{
    TaskSupplier::Close();
    
    //Destroy the VDA decoder if instantiated.
    if(m_isVDAInstantiated)     {
        VDADecoderFlush(m_VDADecoder, 0);
        VDADecoderDestroy(m_VDADecoder);  
        m_isVDAInstantiated = false;
    }
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
            return 0;

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
    
//Create the VDADecoder
OSStatus VDATaskSupplier::CreateDecoder(SInt32 inHeight, SInt32 inWidth, OSType inSourceFormat, CFDataRef inAVCCData, VDADecoder *decoderOut, void * userContextInfo)  
{
    
    OSStatus status;
    
    CFMutableDictionaryRef decoderConfiguration = NULL;
    CFMutableDictionaryRef destinationImageBufferAttributes = NULL;
    CFDictionaryRef emptyDictionary; 
    
    CFNumberRef height = NULL;
    CFNumberRef width= NULL;
    CFNumberRef sourceFormat = NULL;
    CFNumberRef pixelFormat = NULL; 
   
    // source must be H.264
    if (inSourceFormat != 'avc1') {
        printf(" VDATaskSupplier::%s Source format is not H.264!\n", __FUNCTION__);
        return paramErr;
    }
    
    // the avcC data chunk from the bitstream must be present
    // From http://developer.apple.com/library/mac/#technotes/tn2267/_index.html:
    //    "kVDADecoderConfiguration_avcCData - A CFDataRef containing avcC data from the H.264 bitstream. 
    //     In a QuickTime movie file, this is the same data which is stored in the image description as the avcC atom."
    if (inAVCCData == NULL) {
        printf(" VDATaskSupplier::%s avc decoder configuration data cannot be NULL!\n", __FUNCTION__);
        return paramErr;
    }
    
    // create a CFDictionary describing the source material for decoder configuration
    decoderConfiguration = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                     4,
                                                     &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);
    
    height = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inHeight);
    width = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inWidth);
    sourceFormat = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inSourceFormat);
    
    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_Height, height);
    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_Width, width);
    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_SourceFormat, sourceFormat);
    CFDictionarySetValue(decoderConfiguration, kVDADecoderConfiguration_avcCData, inAVCCData);    
    
    // create a CFDictionary describing the wanted destination image buffer
    destinationImageBufferAttributes = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                 2,
                                                                 &kCFTypeDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks);
    
    OSType cvPixelFormatType = kCVPixelFormatType_420YpCbCr8Planar;      
    pixelFormat = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &cvPixelFormatType);
    emptyDictionary = CFDictionaryCreate(kCFAllocatorDefault, // our empty IOSurface properties dictionary
                                         NULL,
                                         NULL,
                                         0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    
    CFDictionarySetValue(destinationImageBufferAttributes, kCVPixelBufferPixelFormatTypeKey, pixelFormat);
    CFDictionarySetValue(destinationImageBufferAttributes, kCVPixelBufferIOSurfacePropertiesKey, emptyDictionary);
    
    // create the hardware decoder object
    status = VDADecoderCreate(decoderConfiguration,
                              destinationImageBufferAttributes, 
                              (VDADecoderOutputCallback *) &((TaskBrokerSingleThreadVDA *)m_pTaskBroker)->decoderOutputCallback2,
                              (void *) userContextInfo,
                              decoderOut);
    
    if (kVDADecoderNoErr != status) {
        
        printf(" VDATaskSupplier::%s VDADecoderCreate failed. err: %d", __FUNCTION__, (int) status);
        switch (status) {
            case kVDADecoderHardwareNotSupportedErr:
                printf(", the hardware does not support accelerated video services required for hardware decode\n");
                break;
            case kVDADecoderFormatNotSupportedErr:
                printf(", the hardware may support accelerated decode, but does not support the requested output format\n");
                break;
            case kVDADecoderConfigurationError:
                printf(", invalid or unsupported configuration parameters were specified in VDADecoderCreat\n");
                break;
            case kVDADecoderDecoderFailedErr:
                printf(", an error was returned by the decoder layer. This may happen for example because of bitstream or data errors during a decode operation. This error may also be returned from VDADecoderCreate when hardware decoder resources are available on the system but currently in use by another process\n");
                break;
            case paramErr:
                printf(", error in user parameter list\n");
                break;
            default:
                printf("\n");
                break;
        }
    }

#ifdef TRY_PROPERMEMORYMANAGEMENT    
    //Release objects nicely
   if (decoderConfiguration)
   {   
      //Removing the values is necessary to decrement the retain count of the values
      CFDictionaryRemoveAllValues(decoderConfiguration);
      CFRelease(decoderConfiguration);
   }
    if (destinationImageBufferAttributes)
   {
      CFDictionaryRemoveAllValues(destinationImageBufferAttributes);
      CFRelease(destinationImageBufferAttributes);
   }
    if (emptyDictionary)
   {
      CFRelease(emptyDictionary);
   }
   
   //more cleanup
   if(height)   
   {
      CFRelease(height);
   }
   if(width)   
   {
      CFRelease(width);
   }
   if(sourceFormat)   
   {
      CFRelease(sourceFormat);
   }
   if(pixelFormat)   
   {
      CFRelease(pixelFormat);
   }
#endif    
    return status;
}

Status VDATaskSupplier::DecodeHeaders(MediaDataEx *nalUnit)
{
    
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
    RawHeader * hdr;
    size_t size;
    Ipp32s id;
    Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
    switch(nal_unit_type)
    {
        case NAL_UT_SPS:
        {
            size = nalUnit->GetDataSize();
            hdr = GetSPS();
            id = m_Headers.m_SeqParams.GetCurrentID();
            hdr->Resize(id, size + sizeof(start_code_prefix));
            memcpy(hdr->GetPointer(), start_code_prefix,  sizeof(start_code_prefix));
            memcpy(hdr->GetPointer() + sizeof(start_code_prefix), (Ipp8u*)nalUnit->GetDataPointer(), size);                   
            hdr->SetRBSPSize(size);                   
            m_isHaveSPS = true;
            if(true == m_isVDAInstantiated)    {
                
                //VDA decoder is already instantiated, is this a new SPS-PPS sequence?
                printf(" VDATaskSupplier::%s WARNING, VDA is instantiated, SPS is in bitstream\n", __FUNCTION__);                                                                
            }
            break;
        }
        case NAL_UT_PPS:
        {
            
         //Add this PPS information to existing PPS information
         size = nalUnit->GetDataSize();
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
                    
                    VideoDecoderParams decoderParams;
                    TaskSupplier::GetInfo(&decoderParams);
                    RawHeader * pSPSHeader = GetSPS();

                    //Prepare the data for VDACreateDecoder
                    CFDataRef avcDataRef;    
                    Ipp8u avcData[1024];
                    int index = 0;
                    
                    avcData[index++] = (Ipp8u) 0x01;                            //Version
                    avcData[index++] = decoderParams.profile;                   //AVC Profile
               
               //14496-15 says that the next byte is defined exactly the same as the byte which occurs
               //between the profile_IDC and level_IDC in a sequence parameter set (SPS).
               const int theByteAfterProfile = 6;
                    avcData[index++] = *(pSPSHeader->GetPointer() + theByteAfterProfile);  //profile_compatibility

                    avcData[index++] = decoderParams.level;                     //AVC Level
                    avcData[index++] = (Ipp8u) 0xff;                            //Length of Size minus one and 6 reserved bits. (4-1 = 3)
                    avcData[index++] = (Ipp8u) 0xe1;                            //Number of Sequence Parameter Sets (hard-coded to 1) and 3 reserved bits
                    
                    //Length of SPS
                    int spsLength = pSPSHeader->GetRBSPSize();
                    Ipp16u bigEndianSPSLength = htons(spsLength);
                    avcData[index++] = (Ipp8u) (bigEndianSPSLength & 0x00ff);         //LSB of SPS Length (expressed in network order)
                    avcData[index++] = (Ipp8u) ((bigEndianSPSLength >> 8) & 0x00ff);  //MSB of SPS Length (expressed in network order)
                    
                    //Copy raw bytes of SPS to this local spot (avcData)
                    const int lengthPreamble = 4;
                    memcpy(&avcData[index], pSPSHeader->GetPointer()+lengthPreamble, spsLength);
                    index += spsLength;                                     
                    
                    avcData[index++] = (Ipp8u) 1;                                     //Number of Picture Parameter Sets (hard-coded to 1)
                    
                    //Length of PPS
                    int ppsLength = size;
                    Ipp16u bigEndianPPSLength = htons(ppsLength);
                    avcData[index++] = (Ipp8u) (bigEndianPPSLength & 0x00ff);         //LSB of PPS Length (expressed in network order)
                    avcData[index++] = (Ipp8u) ((bigEndianPPSLength >> 8) & 0x00ff);  //MSB of PPS Length (expressed in network order)
                    
                    //Copy raw bytes of PPS to this local spot (avcData)
                    memcpy(&avcData[index], hdr->GetPointer()+lengthPreamble, ppsLength);
                    index += ppsLength;
                    
                    avcDataRef = CFDataCreate(kCFAllocatorDefault, avcData, index);       
                    if(NULL == avcDataRef)    {
                        printf(" VDATaskSupplier::%s Error: could not create avcDataRef from avcData Ipp8u array\n", __FUNCTION__);
                        throw h264_exception(UMC_ERR_DEVICE_FAILED);
                    }   
                    else    {
                        
                        OSStatus returnValue;
                        returnValue = CreateDecoder(decoderParams.info.clip_info.height, decoderParams.info.clip_info.width, 'avc1', avcDataRef, &m_VDADecoder, m_pTaskBroker);
                        
                        //If the hardware decoder could not be instantiated then fallback to software decode.
                        if(kVDADecoderNoErr == returnValue)    {
                            m_isVDAInstantiated = TRUE; 
#ifdef TRY_PROPERMEMORYMANAGEMENT
                     if (avcDataRef) {
                        CFRelease(avcDataRef);
                     }
                     else {
                        printf("  VDATaskSupplier::%s avcDataRef no longer exists, release was not attempted.\n", __FUNCTION__);
                     }
#endif
                        }
                        else {
                            printf(" VDATaskSupplier::%s CreateDecoder Error %d, falling back to software decode\n", __FUNCTION__, returnValue);
#ifdef TRY_PROPERMEMORYMANAGEMENT
                     if (avcDataRef) {
                        CFRelease(avcDataRef);
                     }
                     else {
                        printf("  VDATaskSupplier::%s avcDataRef no longer exists, release was not attempted.\n", __FUNCTION__);
                     }
#endif
                            throw h264_exception(UMC_ERR_DEVICE_FAILED);
                        }                
                    }
                }
                else    {
                    
                    //VDA decoder is already instantiated, is this a new SPS-PPS sequence?
                    printf(" VDATaskSupplier::%s WARNING, VDA is instantiated, PPS is in bitstream\n", __FUNCTION__);                                            
                }
            }
            else    {
                
                //Odd, the NAL is a PPS but we don't have the SPS yet!
                printf(" VDATaskSupplier::%s WARNING, have PPS but have not seen SPS yet\n", __FUNCTION__);                    
            }
        }
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

Status VDATaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
   
    static Ipp8u start_code_prefix[] = {0, 0, 1};

    if(NULL == pFrame)  {
        return UMC_OK;
    }
   
   H264DecoderFrameInfo *slicesInfo = pFrame->GetAU(field);
   if (slicesInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED) {
        return UMC_OK;
   }
   
    Status baseStatus;
    baseStatus = TaskSupplier::CompleteFrame(pFrame, field);
    if (UMC_OK != baseStatus) {
        return baseStatus;
    }
    
   H264DecoderFrameInfo * sliceInfo = pFrame->GetAU(field);

    OSStatus status;
    static Ipp32s frameCounter = 0;
   static Ipp32u biggestNAL = 0;
    Ipp32s frameType = pFrame->m_FrameType; 
    Ipp32s nalType;
    Ipp32s index = pFrame->m_index;
    CFDataRef compressedFrame;
    CFMutableDictionaryRef frameInfo = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFNumberRef cfnrFrameCounter;
    CFNumberRef cfnrFrameType;
    CFNumberRef cfnrNalType;
    CFNumberRef cfnrIndex;
   
   Ipp8u *pAVCCFormat;                  //VDA wants the size in network order, then the nal data. This buffer will contain that. 

    for (Ipp32u i = 0; i < sliceInfo->GetSliceCount(); i++)
    {
        H264Slice *pSlice = sliceInfo->GetSlice(i);
            
        Ipp8u *pNalUnit;    //ptr to first byte of start code
        Ipp32u NalUnitSize; // size of NAL unit in bytes

        pSlice->GetBitStream()->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
      
      if(NalUnitSize > biggestNAL)   {
         
         biggestNAL = NalUnitSize;
      }

      if((pAVCCFormat = (Ipp8u *) malloc(NalUnitSize + sizeof(int))) == NULL)   {
            printf("  VDATaskSupplier::%s could not malloc memory for NAL buffer\n", __FUNCTION__);
            return UMC_ERR_NULL_PTR;
      }
      memcpy(&pAVCCFormat[4], pNalUnit, NalUnitSize);
        *((int * ) pAVCCFormat) = htonl(NalUnitSize);
        
        compressedFrame = CFDataCreate(kCFAllocatorDefault, pAVCCFormat, NalUnitSize+sizeof(int));
      free(pAVCCFormat);
      
        if(NULL == compressedFrame)   {
            printf("  VDATaskSupplier::%s CFDataCreate returned NULL pointer\n", __FUNCTION__);
            return MFX_ERR_ABORTED;      
        }
                
        //Setup the context information for this NAL
        frameCounter++;   
        cfnrFrameCounter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameCounter);
        CFDictionarySetValue(frameInfo, CFSTR("vdaFrameCounter"), cfnrFrameCounter);
        
        cfnrFrameType = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameType);
        CFDictionarySetValue(frameInfo, CFSTR("vdaFrameType"), cfnrFrameType);
        
        nalType = pNalUnit[0] & NAL_UNITTYPE_BITS;
        cfnrNalType = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &nalType);
        CFDictionarySetValue(frameInfo, CFSTR("vdaNalType"), cfnrNalType);        
                
        cfnrIndex = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &index);
        CFDictionarySetValue(frameInfo, CFSTR("vdaIndex"), cfnrIndex);

        //Give this to the VDA decoder
        status = VDADecoderDecode(m_VDADecoder, 0, compressedFrame, frameInfo); 
        if (kVDADecoderDecoderFailedErr == status) {
            
            printf(" VDATaskSupplier::%s VDADecoderDecode failed, return code = %d\n", __FUNCTION__, status);

#ifdef TRY_PROPERMEMORYMANAGEMENT
         if (frameInfo) {
            CFDictionaryRemoveAllValues(frameInfo);
            CFRelease(frameInfo);
         }
         if (compressedFrame) {
            CFRelease(compressedFrame);
         }
         if (cfnrFrameCounter) {
            CFRelease(cfnrFrameCounter);
         }
         if (cfnrFrameType) {
            CFRelease(cfnrFrameType);
         }
         if (cfnrNalType) {
            CFRelease(cfnrNalType);
         }
         if (cfnrIndex) {
            CFRelease(cfnrIndex);
         }
#endif         
            return  MFX_ERR_ABORTED;
        }
        m_pTaskBroker->AddFrameToDecoding(pFrame);

      if (compressedFrame) {
         CFRelease(compressedFrame);
      }
      
#ifdef TRY_PROPERMEMORYMANAGEMENT      
      if (frameInfo) {
//         CFDictionaryRemoveAllValues(frameInfo);
      }
      if (cfnrFrameCounter) {
         CFRelease(cfnrFrameCounter);
      }
      if (cfnrFrameType) {
         CFRelease(cfnrFrameType);
      }
      if (cfnrNalType) {
         CFRelease(cfnrNalType);
      }
      if (cfnrIndex) {
         CFRelease(cfnrIndex);
      }
#endif      
    }   
#ifdef TRY_PROPERMEMORYMANAGEMENT   
   if (frameInfo) {
      CFRelease(frameInfo);
   }
#endif
   
    return UMC_OK;
}

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
//    printf("TaskBrokerSingleThreadVDA::%s Entry\n", __FUNCTION__);
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
   if(!frameInfo)   {
        printf(" TaskBrokerSingleThreadVDA::%s called with NULL pointer to context information!", __FUNCTION__);
        return;
    }
    
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
      CFRelease(frameInfo);
    }
    else {
        printf("TaskBrokerSingleThreadVDA::%s called with NULL pointer to context information!", __FUNCTION__);
        return;
    }
    
    if (NULL == imageBuffer) {
//        printf("TaskBrokerSingleThreadVDA::%s Hardware Decoder Callback called - NULL image buffer, nal type = %d\n", __FUNCTION__, nalTypeValue);
      printf(" TaskBrokerSingleThreadVDA::%s NULL image buffer! status = %d, flags = %#x, frameType = %d, frameCounter = %d, nalType = %d, imageBuffer %p", __FUNCTION__, status, infoFlags, frameTypeValue, frameCounterValue, nalTypeValue, imageBuffer);
        if (kVDADecoderDecoderFailedErr & infoFlags) {
            printf("TaskBrokerSingleThreadVDA::%s - Decoder Failed!\n", __FUNCTION__);
        }
#if 0    //RMH removed, can't find definition of kVDADecodeInfo_FrameDropped
        if (kVDADecodeInfo_FrameDropped & infoFlags) {
            printf("TaskBrokerSingleThreadVDA::%s - frame dropped!\n", __FUNCTION__);
        }
#endif        
        
        return;
    }
    
    ViewItem &view = *m_pTaskSupplier->GetViewByNumber(BASE_VIEW);
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

        printf(" TaskBrokerSingleThreadVDA::%s Did not find the frame with m_index = %d in the DPB\n", __FUNCTION__, indexValue);
    }

    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    OSType pixelFormatType = CVPixelBufferGetPixelFormatType(imageBuffer);
    
    if((kVDADecoderNoErr == status) && bFoundFrame)
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
        
        //Copy Y        
        yIndex = 0;                  
        bufferHeight = bufferHeightY;
        bufferWidth = bufferWidthY;
        bytesPerRow = bytesPerRowY;
        pPixels = pDecodedY;

        mfxU8 * pDestination = pFrame->m_pYPlane;  
        mfxU8 * pOther = pFrame->m_pYPlane_base;
        for(row = 0; row < bufferHeight; row++)   {
         for(column = 0, yIndex = 0; column < bufferWidth; column++, yIndex++)   {
            pDestination[yIndex] = (pPixels)[column];
            }
            pDestination += pFrame->pitch_luma();
            pPixels += bytesPerRow;
        }
       
        //Copy U and V
        uvIndex = 0;
        bufferHeight = bufferHeightU;
        bufferWidth = bufferWidthU;
        bytesPerRow = bytesPerRowU;
        pPixelsU = pDecodedU;
        pPixelsV = pDecodedV;

      pDestination = pFrame->m_pUVPlane;
        for(row = 0; row < bufferHeight; row++)   {
            for(column = 0, uvIndex = 0; column < bufferWidth; column++, uvIndex += 2)   {
                pDestination[uvIndex] = (pPixelsU)[column];
                pDestination[uvIndex+1] = (pPixelsV)[column];
            }
            pPixelsU += bytesPerRow;
            pPixelsV += bytesPerRow;
         pDestination += pFrame->pitch_chroma();
        }
        
        CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
      
        AutomaticUMCMutex guard(m_mGuard);
        
        //Field or frame?
        Ipp32u field;
      if((pFrame->m_displayPictureStruct == UMC::DPS_TOP) || (pFrame->m_displayPictureStruct == UMC::DPS_BOTTOM))   {
            //it is a field, not frame
            field = 1;
        }
        else    {
            
            //it is a frame, not field
            field  = 0;
        }
      
        ReportItem reportItem(pFrame->m_index, 0, ERROR_FRAME_NONE);
        
        m_reports.push_back(reportItem);
      
      //Progressive or Interlaced?
      if (!field)
      {
         
         //Progressive
//         pFrame->setPicOrderCnt(0, 0);
//          pFrame->setPicOrderCnt(1, 1);
      }
      else
      {
         
         //Interlaced
          ReportItem reportItemBottomField(pFrame->m_index, 1, ERROR_FRAME_NONE/*H264DecoderFrameInfo::STATUS_FILLED*/); 
          m_reports.push_back(reportItemBottomField);
         
//         pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
//         pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
      }
    }
}
    
void TaskBrokerSingleThreadVDA::decoderOutputCallback2(void *decompressionOutputRefCon, CFDictionaryRef frameInfo, OSStatus status, uint32_t infoFlags, CVImageBufferRef imageBuffer)    
{
#ifdef TRY_PROPERMEMORYMANAGEMENT
    if (frameInfo)
    {
        CFRetain(frameInfo);        
    }
   if (imageBuffer)
    {
        CFRetain(imageBuffer);        
    }
#endif
    TaskBrokerSingleThreadVDA * pTaskBrokerSingleThreadVDA = (TaskBrokerSingleThreadVDA *) decompressionOutputRefCon;
    pTaskBrokerSingleThreadVDA->decoderOutputCallback(frameInfo, status, infoFlags, imageBuffer);
#ifdef TRY_PROPERMEMORYMANAGEMENT
    if (imageBuffer)
    {
        CFRelease(imageBuffer);        
    }
    if (frameInfo)
    {
        CFRelease(frameInfo);        
    }
#endif    
}    

bool TaskBrokerSingleThreadVDA::GetNextTaskInternal(H264Task *)
{
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

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H264_VIDEO_DECODER
#endif // __APPLE__
