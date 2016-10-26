//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#ifdef __APPLE__
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_VDA_SUPPLIER_H
#define __UMC_H264_VDA_SUPPLIER_H

#include "umc_h264_dec_mfx.h"
#include "umc_h264_mfx_supplier.h"
#include "umc_h264_segment_decoder_dxva.h"
#include "umc_mutex.h"

#include <VTDecompressionSession.h>
#include <CVImageBuffer.h>

//NOTE: If you define VTB_LOAD_LIB_AT_INIT, then remove VideoToolbox from the frameworks linked at build time. 
//#define VTB_LOAD_LIB_AT_INIT

//#define USE_APPLEGVA

#ifdef USE_APPLEGVA

#include <CoreVideo/CVPixelBuffer.h>
#include <deque>

class TaskBrokerSingleThreadVDA;

//AppleGVA function return codes
enum _APPLEGVA_FUNCTION_RETURN_CODES
{
    APPLEGVA_RETURN_PASS = 0,
    APPLEGVA_RETURN_FAIL = 7
};

const unsigned int maxNumberFrameContexts = 1024;
struct _decodeFrameContext   {
    
    unsigned int frameNumber;
    unsigned int frameType;
    H264DecoderFrame * pFrame;
};

struct _decodeCallbackInformation   {
    
    TaskBroker * pTaskBrokerSingleThreadVDA;
    struct _decodeFrameContext arrayFrameContexts[maxNumberFrameContexts];
    
};

//AppleGVA Function 14 Parameter 5 is a pointer to a pointer to a NALDescriptor structure
typedef struct _NALDescriptor
{
    uint32_t sizeNAL;
    uint32_t reserved;
    unsigned char * pNALData;
} NALDescriptor;

//AppleGVA Decoder Statistics
typedef struct _decoderStatistics
{
    uint32_t framesReceived;
    uint32_t framesSkipped;
    uint32_t framesDegraded;
    uint32_t framesDecoded;
} decoderStatistics;

//In qt test app, prepareMainCallbacks mallocs and initializes three callbackInformation structures,
//and mallocs and initializes three callbackIdentifier structures.
//prepareMainCallbacks returns a pointer to an arry of pointers. Each pointer in that array points to a
//callbackIdentifier structure. The array of pointers was also malloc'd.
typedef struct _callbackInformation
{
    void * pContextInformation;
    void (* callbackFunction) (void *, void *);
    
} callbackInformation;

typedef struct _callbackIdentifier
{
    uint32_t idNumber;
    uint32_t reserved;
    callbackInformation * pCallbackInformation;
} AVF_Callback;

//AppleGVA uses these values when initializing AVF_Callback structures
enum _APPLEGVA_CALLBACK_ID
{
    APPLEGVA_CALLBACK_STOP = 1,
    APPLEGVA_CALLBACK_FLUSH = 2,
    APPLEGVA_CALLBACK_BUFFER_REQUIREMENT = 3,
    APPLEGVA_CALLBACK_MAIN_QUERY_SYSTEMCLOCK = 4,
    APPLEGVA_CALLBACK_MAIN_OUTPUT_QUEUE_READY = 5,
    APPLEGVA_CALLBACK_VIDEO_DECODER_SEQUENCE = 6,
    APPLEGVA_CALLBACK_CONTEXT_CREATION = 9,
    APPLEGVA_CALLBACK_MAIN_DECODED_PICTURE_READY = 12
};

//This is the structure passed to DecodedPictureReadyCallback by AppleGVA`___lldb_unnamed_function739$$AppleGVA (0x100048888)
//I know that it cannot be longer than 36 bytes, based on the stack allocation made in function739.
//I see that qt's DecodedPictureReadyCallback only accesses the first 32 bytes. 
struct AVF_PixelBufferInfo
{
    uint64_t offset_0_7;
    CVPixelBufferRef pixelBuffer;
    uint64_t offset_16_23;
    union   {
        
        uint64_t offset_24_31;
        struct {
            
            uint32_t offset_24_27;
            uint32_t offset_28_31;
        };
    };
};

//This structure is new'd by CreateAccelerator.
//CreateAccelerator stores a pointer to it in the pointer passed as parameter 3.
//I've been storing that pointer at gvaInformation.createAcceleratorParameter3.
//I've identified the members that I know
const unsigned int sizeStructureAllocatedInitializedByCreateAccelerator = 1504;

struct _structureAllocatedInitializedByCreateAccelerator
{
    char unknown0_15[16];
    pthread_mutex_t createStructureMutex;                   //offset 16, mutex size = 64. This mutex protects this structure.
    char unknown80_759[760-(64+16)];
    void * pContextInformation_QuerySystemClockCallback;    //offset 760
    void * pContextInformation_QQR_and_DPRCallbacks;        //offset 768
    void (* pQuerySystemClockCallback) (void *, void *);    //offset 776
    void (* pQueryQueyReadyCallback) (void *, void *);      //offset 784, QQR
    void (* pDecodePictureReadyCallback) (void *, void *);  //offset 792, DPR
    void (* pUnknown1Callback) (void *, void *);            //offset 800, Unknown callback 1 (an educated guess that these next 8 are also callbacks)
    void (* pUnknown2Callback) (void *, void *);            //offset 808, Unknown callback 2
    void (* pUnknown3Callback) (void *, void *);            //offset 816, Unknown callback 3
    void (* pUnknown4Callback) (void *, void *);            //offset 824, Unknown callback 4
    void (* pUnknown5Callback) (void *, void *);            //offset 832, Unknown callback 5
    void (* pUnknown6Callback) (void *, void *);            //offset 840, Unknown callback 6
    void (* pUnknown7Callback) (void *, void *);            //offset 848, Unknown callback 7
    void (* pUnknown8Callback) (void *, void *);            //offset 856, Unknown callback 8
    pthread_mutex_t decodeMutex1;                           //offset 864, mutex size = 64
    pthread_cond_t decodeConditionVariable1;                //offset 928, condition variable size = 48
    pthread_mutex_t decodeMutex2;                           //offset 976, mutex size = 64
    pthread_cond_t decodeConditionVariable2;                //offset 1040
    char unknown1088_1503[sizeStructureAllocatedInitializedByCreateAccelerator-1088];
};

//This is my version of qt's testapp_info
struct _gvaInformation
{
    AVF_Callback * * arrayPointersOtherCallbackIdentifiers;    //rbp_minus6696
    uint32_t rbp_minus6688;             //address is passed to Function 5, CreateVideoBitstreamPool
    uint32_t rbp_minus6680;             //1 of 2 cleared just before invoking Func22 (GetScanMode)
    uint32_t rbp_minus6676;             //2 of 2 cleared just before invoking Func22 (GetScanMode)
    uint32_t queryRenderModeParameter3; //rbp_minus6672
    uint32_t rbp_minus6668;             //rbp_minus6668
    void * * createAcceleratorParameter3; //rbp_minus6664: CreateAccelerator news this memory. Points to a _structureAllocatedInitializedByCreateAccelerator
    CGDirectDisplayID quartzDisplayID;  //rbp_minus6656
    uint32_t valueAfterDisplayID;       //rbp_minus6652
    uint32_t width;                     //rbp_minus6648, I had been calling this unkown_offset8
    uint32_t height;                    //rbp_minus6644, I had been calling this unkown_offset12
    char unknownSpace[24];              //rbp_minus6640 to rbp_minus6617. The array of pointers to callback identifiers needs to be 48 bytes away from quartzDisplayID
    
    uint32_t numberOfMainCallbacks;     //rbp_minus6616
    uint32_t rbp_minus6612;             //unknown
    
    //The pointer to the array of pointers to main callbackIdentifiers needs to be 48 bytes away from quartzDisplayID
    AVF_Callback * * arrayPointersMainCallbackIdentifiers;    //rbp_minus6608
    char rbp_minus6600_6581[20];        //This starts 64 bytes away from createAcceleratorParameter3
};

//_AVF_MediaAcceleratorInterface *


struct _functionStructure
{
    uint32_t id;
    uint32_t reserved;
    uint64_t prepareFunctions_parameter_4;
    union {
        uint32_t asInteger;
        char format_1_and_2[8]; //actually two arrays of 4 bytes of characters
    };
};

struct _prepareStructure
{
    uint32_t valueInitializedBy_prepareFunctions;               //rbp_minus6720
    uint32_t reserved;
    struct _functionStructure * * pMallocBy_prepareFunctions;   //rbp_minus6712
    uint32_t r15_plus16;                                        //rbp_minus6704
    void * r15_plus24;                                          //rbp_minus6700
};

enum _SetDecodeDescriptionCommands
{
    SETDECODE_DI_EBL = 0,           //Not sure what di ebl is. Takes two parameters
    SETDECODE_WIDTH = 3,
    SETDECODE_FRAMERATE = 4,
    SETDECODE_ENABLE_FMBS = 5,
    SETDECODE_ENABLE_FMSS = 6,
    SETDECODE_OUTPUT_ORDER = 7,
    SETDECODE_BUFFER_COUNT = 10,
    SETDECODE_REORDER_DELAY = 11,
    SETDECODE_HEIGHT = 17,
};

//Callback functions
void QuerySystemClockCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);
void OutputQueueReadyCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);
void DecodedPictureReadyCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);
void StopCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);
void FlushCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);
void BufferRequirementCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);
void VideoDecoderSequenceInfoCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);
void ContextCreationInfoCallback(void * pApplicationContextInformation, void * pSpecificCallbackHandlerInformation);

#endif //USE_APPLEGVA

namespace UMC
{

const char * getVTErrorString(OSStatus errorNumber);

class MFXVideoDECODEH264;

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
    class VDATaskSupplier : public MFXTaskSupplier, public DXVASupport<VDATaskSupplier>

{
    friend class TaskBroker;
    friend class DXVASupport<VDATaskSupplier>;    
    friend class VideoDECODEH264;

public:

    VDATaskSupplier();
    ~VDATaskSupplier();
    
    struct nalStreamInfo    {
        
        Ipp32u nalNumber;
        Ipp32u nalType;
        std::vector<Ipp8u> headerBytes;
    };

    virtual Status Init(BaseCodecParams *pInit);
    virtual void Close();
    void SetBufferedFramesNumber(Ipp32u buffered);
    virtual Status AddSource(MediaData * pSource, MediaData *dst);

protected:
    VTDecompressionSessionRef m_decompressionSession;
    CMVideoFormatDescriptionRef m_sourceFormatDescription;
    
#ifdef VTB_LOAD_LIB_AT_INIT
    void * m_libHandle;
    
    //Function declarations for pointers to VideoToolbox functions, the VTB framework is loaded dynamically.
    OSStatus (* call_VTDecompressionSessionCreate) (CFAllocatorRef,
                                                    CMVideoFormatDescriptionRef,
                                                    CFDictionaryRef,
                                                    CFDictionaryRef,
                                                    const VTDecompressionOutputCallbackRecord *,
                                                    VTDecompressionSessionRef *);
    void (* call_VTDecompressionSessionInvalidate) (VTDecompressionSessionRef);
    CFTypeID (* call_VTDecompressionSessionGetTypeID) (void);
    OSStatus (* call_VTDecompressionSessionDecodeFrame) (VTDecompressionSessionRef,
                                                         CMSampleBufferRef,
                                                         VTDecodeFrameFlags,
                                                         void *,
                                                         VTDecodeInfoFlags *);
    OSStatus (* call_VTDecompressionSessionFinishDelayedFrames) (VTDecompressionSessionRef);
    Boolean (* call_VTDecompressionSessionCanAcceptFormatDescription) (VTDecompressionSessionRef, CMFormatDescriptionRef);
    OSStatus (* call_VTDecompressionSessionWaitForAsynchronousFrames) (VTDecompressionSessionRef);
    OSStatus (* call_VTDecompressionSessionCopyBlackPixelBuffer) (VTDecompressionSessionRef, CVPixelBufferRef);
    //End of VideoToolbox function pointer declarations.
#endif //ifdef VTB_LOAD_LIB_AT_INIT
    
    bool m_isVDAInstantiated; 
    bool m_isHaveSPS;           //flag indicating whether or not the raw SPS bytes are available
    bool m_isHavePPS;           //flag indicating whether or not the raw PPS bytes are available
    
    virtual Status DecodeSEI(MediaDataEx *nalUnit);
    virtual Status RunDecoding(bool force, H264DecoderFrame ** decoded = 0);

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat chroma_format_idc);

    virtual Status CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field);

    virtual H264DecoderFrame * GetFreeFrame(const H264Slice *pSlice = NULL);

    virtual H264Slice * DecodeSliceHeader(MediaDataEx *nalUnit);

    virtual Status DecodeHeaders(MediaDataEx *nalUnit);

private:
    Ipp32u m_bufferedFrameNumber;

    H264DecoderFrame * m_pLastDecodedFrame;   //needed for cut and paste from VATaskSupplier::CompleteFrame
    std::vector<Ipp8u> m_nals;
    std::vector<Ipp8u> m_avcData;
    unsigned int m_ppsCountIndex;
    std::queue<struct nalStreamInfo> m_prependDataQueue;
    unsigned int m_sizeField;
    H264DecoderFrame * m_pFieldFrame;
    void AddToNALQueue(struct nalStreamInfo nalInfo);
    UMC::Mutex m_nalQueueMutex;
    
#ifdef USE_APPLEGVA
    
    static const unsigned int m_mainCallbackCount = 3;
    static const unsigned int m_otherCallbackCount = 5;
    static const unsigned int m_sizeNALDescriptorPool = 16;
    static const unsigned int m_poolIndexMask = 0x0f;
    unsigned int m_gvaIndex = 0;

    struct _gvaInformation m_gvaInformation;
    bool m_bVideoBitstreamPoolExists;
    std::deque<struct _decodeFrameContext> m_callbackContext;
    struct _decodeCallbackInformation m_callbackContextInformation;
//    struct _decodeFrameContext m_arrayCallbackContext[maxNumberFrameContexts];
    unsigned int m_callbackIndex = 0;
    
    void * * m_mediaAcceleratorInterface = NULL;
    void * m_libHandleAppleGVA;
    unsigned char *m_pNALBuffer = NULL;
    NALDescriptor * * m_NALDescriptorPool = NULL;
    unsigned int m_poolIndex = 0;
    Status loadAppleGVALibrary(void);
    void freeCallbackStructures(unsigned int callBackIdentifierCount, AVF_Callback * * arrayPointersCallbackIdentifiers);
    struct _functionStructure * * prepareFunctions(uint32_t parameter_1, uint32_t * pOutput, uint32_t parameter_3, uint32_t parameter_4, uint32_t parameter_5, uint32_t parameter_6);
    AVF_Callback * * prepareMainCallbacks(unsigned int callBackIdentifierCount, struct _gvaInformation * p_testapp_info);
    AVF_Callback * * prepareCallbacks(unsigned int callBackIdentifierCount, struct _gvaInformation * p_testapp_info);
    
    void initializeAppleGVAMembers(void);
    void releaseNALDescriptorPool(void);
    int (* m_pCreateAccelerator) (int32_t, uint32_t *, void * * *);
    int (* m_pDestroyAccelerator) (void *);
    int (* m_pDisposeVideoBitstreamPool) (void *, uint32_t);
    int (* m_pQueryRenderMode) (void *, uint64_t, uint32_t *);
    int (* m_pSetScanMode) (void *, uint64_t, uint32_t *);
    int (* m_pCreateVideoBitstreamPool) (void *, uint32_t, uint32_t *);
    int (* m_pCreateMainVideoDataPath) (void *, uint32_t, struct _prepareStructure *);
    int (* m_pSetDecodeDescription) (void *, uint32_t, uint32_t, uint32_t, void *);
    int (* m_pGetNALBufferAddress) (void *, uint32_t, uint32_t, unsigned char * *);         //AppleGVA Function 8
    int (* m_pSetNALBufferReady) (void *, uint32_t, uint32_t);                              //AppleGVA Function 13
    int (* m_pSetNALDescriptors) (void *, uint32_t, uint32_t, uint32_t, NALDescriptor * *); //AppleGVA Function 14
    int (* m_pFunction15) (void *, uint32_t, uint32_t, NALDescriptor * *);                  //AppleGVA Function 15, not sure what parm 4 points to
    int (* m_pGetDecoderStatistics) (void *, uint32_t, uint32_t, uint32_t, uint32_t *);
    
#endif //USE_APPLEGVA
};

class TaskBrokerSingleThreadVDA : public TaskBrokerSingleThread
{
public:
    TaskBrokerSingleThreadVDA(TaskSupplier * pTaskSupplier);

    virtual bool PrepareFrame(H264DecoderFrame * pFrame);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);
    virtual void Start();

    virtual void Reset();
    void decoderOutputCallback(CFDictionaryRef frameInfo, OSStatus status, uint32_t infoFlags, CVImageBufferRef imageBuffer);
    static void decoderOutputCallback2(void *decompressionOutputRefCon,
                                       void * sourceFrameRefCon,
                                       OSStatus status,
                                       VTDecodeInfoFlags infoFlags,
                                       CVImageBufferRef imageBuffer,
                                       CMTime presentationTimeStamp,
                                       CMTime presentationDuration);
    
    virtual void AddReportItem(Ipp32u index, Ipp32u field, Ipp8u status);

protected:
    virtual void AwakeThreads();


    class ReportItem
    {
    public:
        Ipp32u  m_index;
        Ipp32u  m_field;
        Ipp8u   m_status;

        ReportItem(Ipp32u index, Ipp32u field, Ipp8u status)
            : m_index(index)
            , m_field(field)
            , m_status(status)
        {
        }

        bool operator == (const ReportItem & item)
        {
            return (item.m_index == m_index) && (item.m_field == m_field);
        }

        bool operator != (const ReportItem & item)
        {
            return (item.m_index != m_index) || (item.m_field != m_field);
        }
    };

    typedef std::vector<ReportItem> Report;
    Report m_reports;
    Ipp64u m_lastCounter;
    Ipp64u m_counterFrequency;
};

class H264_VDA_SegmentDecoder : public H264SegmentDecoderMultiThreaded
{
public:
    H264_VDA_SegmentDecoder(TaskSupplier * pTaskSupplier);

    virtual Status ProcessSegment(void);
};

} // namespace UMC


#endif // __UMC_H264_VDA_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
#endif // __APPLE__
