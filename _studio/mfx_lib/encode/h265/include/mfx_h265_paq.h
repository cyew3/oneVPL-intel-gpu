//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_H265_PAQ)

#ifndef __MFX_H265_PAQ_H__
#define __MFX_H265_PAQ_H__

#include <stdio.h>
#include "mfxstructures.h"
#include "mfx_h265_defs.h"

namespace H265Enc {

    //*****************************************************
    //  LookAhead Interface
    //*****************************************************

    class AsyncRoutineEmulator
    {
    public:
        enum {
            STG_ACCEPT_FRAME,
            STG_START_LA,
            STG_WAIT_LA,
            STG_START_ENCODE,
            STG_WAIT_ENCODE,
            STG_COUNT
        };

        enum {
            STG_BIT_CALL_EMULATOR = 0,
            STG_BIT_ACCEPT_FRAME  = 1 << STG_ACCEPT_FRAME,
            STG_BIT_START_LA      = 1 << STG_START_LA,
            STG_BIT_WAIT_LA       = 1 << STG_WAIT_LA,
            STG_BIT_START_ENCODE  = 1 << STG_START_ENCODE,
            STG_BIT_WAIT_ENCODE   = 1 << STG_WAIT_ENCODE,
            STG_BIT_RESTART       = 1 << STG_COUNT
        };

        AsyncRoutineEmulator();

        AsyncRoutineEmulator(mfxVideoParam const & video);

        void Init(mfxVideoParam const & video);

        mfxU32 GetTotalGreediness() const;

        mfxU32 GetStageGreediness(mfxU32 i) const;

        mfxU32 Go(bool hasInput);

    protected:
        mfxU32 CheckStageOutput(mfxU32 stage);

    private:
        mfxU32 m_stageGreediness[STG_COUNT];
        mfxU32 m_queueFullness[STG_COUNT + 1];
        mfxU32 m_queueFlush[STG_COUNT + 1];
    };

    //*****************************************************
    //   PAQ UTILS
    //*****************************************************

    typedef bool         NGV_Bool;

    // Motion vectors
    typedef struct {
        Ipp16s    iMVx;
        Ipp16s    iMVy;
    } T_ECORE_MV;

    typedef int DIR;
    enum                        {forward, backward, average};

    typedef int RES;
    enum                        {IntRes, HalfRes, QuRes};

#define NgvMemProfile(str)  

#define NgvPrintf 
#define NgvFprintf
#define Ngvthreadprintf 


#define MAX(a,b)            (((a) > (b)) ? (a) : (b))

//#define RV_CDECL               __cdecl
//#define RV_FASTCALL            __fastcall

#define NOTREFERENCED_PARAMETER(P) (P)

//#define NGV_ALIGN(val, ...) __declspec(align(val)) __VA_ARGS__
//#define NGV_ALIGN_ATR(val)

#define NgvMalloc(bytes) malloc(bytes)
#define NgvCalloc(n, bytes) calloc(n, bytes)

//#define NgvMalloc_aligned(bytes, alignment) _aligned_malloc(bytes, alignment)
//#define NgvFree_aligned(ptr) _aligned_free(ptr)
#define NgvMalloc_aligned(bytes, alignment) ippMalloc(bytes)
#define NgvFree_aligned(ptr) ippFree(ptr)

#define MAX_TILE_SIZE 64
#define MB_WIDTH 64
#define REGION_GRID_SIZE 16

#define USE_PDMV 1

    typedef enum Struct_PicCod_Type
    {
        RV_IPIC,    // 0
        RV_PPIC,    // 1
        RV_FPIC,    // 2
        RV_NPIC,    // Not coded
        RV_DUMY,
        NUM_PIC_TYPES
    } EnumPicCodType;

#undef  NULL
#define NULL                0

#define BLOCK_SIZE            4
#define SEARCH_RANGE        16
#define MBLOCK_SIZE            8
#define LN2                    0.6931471805599453
#define INT_MAX                2147483647

#define PI                    3.14159265
#define MAXERROR            65025.0
#define MAXPDIST            5
#define RsCsSIZE            16
#define PD_LIMIT            5000000
#define NumTSC                10
#define NumSC                10
#define IFRAME                0
#define PFRAME                1
#define FFRAME                2
#define MAX_LOOKAHEAD        250

    // elements.h
#define LESS_MEM_PDIST
    //------------

    enum Plane_Type
    {
        PLANE_LUMA,
        PLANE_CR_U,
        PLANE_CR_V
    };

    /**
    * Simple YUV image Class
    */

    struct YuvImage
    {                
        // pitch is same as width
        Ipp32u     width;
        Ipp32u     height;             
        /* This number indicates the temporal position of this image */
        /* with respect to previous and future images.  Its value is */
        /* specific to each video environment. */
        Ipp32u     sequence_number;        
        /*  The Model is such that these Image stuctures are just pointers to actual data */
        /*  So don't just copy the data pointers,. */
        Ipp32u     size;
        Ipp8u*     y_plane;
        Ipp8u*     u_plane;
        Ipp8u*     v_plane;
        Ipp32u     y_pitch;
        Ipp32u     u_pitch;
        Ipp32u     v_pitch;

        Ipp8u*     plane[3];
        Ipp32u     pitch[3];

        Ipp8u      notes;
        NGV_Bool bKeyFrame;
        Ipp32u     system_timestamp;
    } ;
    /**
    * YuvImage 
    */
    YuvImage* YuvImage_Construct(YuvImage* t);

    YuvImage* YuvImage_Destroy(YuvImage* t);

    void YuvImage_Init(YuvImage *t);

    NGV_Bool YuvImage_SetDim(YuvImage *t, Ipp32u width, Ipp32u height);

    void YuvImage_SetPointers(YuvImage* t, Ipp8u* y_plane, Ipp32u width, Ipp32u height);

    void YuvImage_SetAllPointers(YuvImage* t, Ipp8u* y_plane, Ipp8u* u_plane, Ipp8u* v_plane, Ipp32u width, Ipp32u height, Ipp32u y_pitch, Ipp32u cr_pitch);

    void YuvImage_SetPlanePointers(YuvImage* t);

    void YuvImage_Copy(YuvImage *t, YuvImage *src);

    struct YuvImageBuffer
    {        
        Ipp32u         m_uBufferSize;
        Ipp8u*         m_pBuffer;
        YuvImage    image;
    } ;

    /**
    * YuvImageBuffer
    * YuvImage is class to hold inforamtion about the image,
    * but it does not allocate any memory.
    * YuvImageBuffer is a helper class.
    */
    YuvImageBuffer* YuvImageBuffer_Construct(YuvImageBuffer* t);

    YuvImageBuffer* YuvImageBuffer_Destroy(YuvImageBuffer* t);

    void YuvImageBuffer_Init(YuvImageBuffer *t);

    NGV_Bool YuvImageBuffer_Alloc(YuvImageBuffer *t, Ipp32u width, Ipp32u height);

    void YuvImageBuffer_Free(YuvImageBuffer *t);

    struct BinPacket
    {
        NGV_Bool bIsValid;
        Ipp32u        ulPacketOffset;
    } ;

#define MAX_NUM_PACKETS 64

    struct BinImage
    {        
        Ipp32u     width;
        Ipp32u     height;
        Ipp32u     size;
        Ipp32u     sequence_number;
        Ipp8u      flags;
        Ipp8u*     data;
        Ipp8u        frameType;
        Ipp32u     system_timestamp;
        Ipp32u        num_packets;
        BinPacket Packets[MAX_NUM_PACKETS];
        Ipp8u     outputAvailable;        
    } ;

    /**
    * BinImage 
    */
    BinImage* BinImage_Construct(BinImage* t);

    BinImage* BinImage_Destroy(BinImage* t);

    void BinImage_Init(BinImage *t);

    void BinImage_SetDim(BinImage *t, Ipp32u width, Ipp32u height);

    void BinImage_SetPointers(BinImage* t, Ipp8u* base, Ipp32u size);

    NGV_Bool BinImage_isBFrame(BinImage* t);
    NGV_Bool BinImage_isPFrame(BinImage* t);
    NGV_Bool BinImage_isKeyFrame(BinImage* t);

    struct BinImageBuffer
    {        
        Ipp32u         m_uBufferSize;
        Ipp8u*         m_pBuffer;
        BinImage    image;
    } ;

    /**
    * BinImageBuffer
    * BinImage is class to hold information about the image,
    * but it does not allocate any memory.
    * YuvImageBuffer is a helper class.
    */
    BinImageBuffer* BinImageBuffer_Construct(BinImageBuffer* t);

    BinImageBuffer* BinImageBuffer_Destroy(BinImageBuffer* t);

    void BinImageBuffer_Init(BinImageBuffer *t);

    NGV_Bool BinImageBuffer_Alloc(BinImageBuffer *t, Ipp32u size);
    void BinImageBuffer_Free(BinImageBuffer *t);

    /**
    * Simple YUV Subband image Class
    */

    struct YuvSubband
    {                
        // pitch is same as width
        Ipp32u     width;
        Ipp32u     height;             
        /* This number indicates the temporal position of this image */
        /* with respect to previous and future images.  Its value is */
        /* specific to each video environment. */
        Ipp32u     sequence_number;        
        /*  The Model is such that these Image stuctures are just pointers to actual data */
        /*  So don't just copy the data pointers,. */
        Ipp32u     size;
        Ipp16s     *y_plane;
        Ipp16s     *u_plane;
        Ipp16s     *v_plane;
        Ipp32u     y_pitch;
        Ipp32u     u_pitch;
        Ipp32u     v_pitch;
        Ipp16s     *plane[3];
        Ipp32u     pitch[3];
    } ;
    /**
    * Subband Image 
    */
    YuvSubband* YuvSubband_Construct(YuvSubband* t);

    YuvSubband* YuvSubband_Destroy(YuvSubband* t);

    void YuvSubband_Init(YuvSubband *t);

    NGV_Bool YuvSubband_SetDim(YuvSubband *t, Ipp32u width, Ipp32u height);

    void YuvSubband_SetPointers(YuvSubband* t, Ipp16s *y_plane, Ipp32u width, Ipp32u height);

    void YuvSubband_SetAllPointers(YuvSubband* t, Ipp16s *y_plane, Ipp16s *u_plane, Ipp16s *v_plane, Ipp32u width, Ipp32u height, Ipp32u y_pitch, Ipp32u cr_pitch);

    void YuvSubband_SetPlanePointers(YuvSubband* t);

#define MAX_RANK_BITS           2
#define MAX_RANK                ((1<<MAX_RANK_BITS)-1)

    typedef enum 
    {
        RFrame_Image,
        RFrame_Recon,
        RFrame_Input,
        RFrame_Types
    } RFrameType;

    // reference frame impl based on rv image
    struct RFrame
    {
        void                *allocated_buffer;
        Ipp32u                 allocated_buffer_size;

        YuvImage            image;    
        YuvImage            padded_image;    

        // extra information whihc makes this rv image a reference frame
        Ipp32u                 padding;
        Ipp32u                 owidth; // original 
        Ipp32u                 oheight;
        Ipp32u                    heightInTiles;
        Ipp32u                    widthInTiles;
        Ipp32u                    heightInRegionGrid;
        Ipp32u                    widthInRegionGrid;

        // used with framelists (temporal properties)
        RFrame *past_frame;
        RFrame *futr_frame;

        // used with the queue (non temporal properties)
        RFrame *prev_frame;
        RFrame *next_frame;

        Ipp32u                 ePicType;
        Ipp32u                 uPicRank;   // Normative
        Ipp32u                 uPicMod;    // Informational
        Ipp32u                 TR;
        Ipp32u                 npTR;
        NGV_Bool             coded;
        NGV_Bool             displayed;
        NGV_Bool             reference;
        NGV_Bool             latency; //To control lookahead

        // Preprocessing Data
        RFrameType          type;   // IMAGE, RECON IMAGE, ENCODER INPUT IMAGE
        void                *sample;
        Ipp32u                 *BSAD;
        Ipp32u                 *PSAD;
        T_ECORE_MV          *PDMV;
        Ipp32u                 *PDSAD;
        T_ECORE_MV          *PMV;
        Ipp32u                 *FDSAD;
        T_ECORE_MV          *FMV;
        Ipp32s                 *qp_mask;
        Ipp32s                 *coloc_past;
        Ipp32s                 *coloc_futr;
        Ipp32s                 *sc_mask;
        Ipp32s                 deltaQpLow;
        Ipp32s                 deltaQpHigh;

        Ipp32s                    Schg;
        double                SC;
        int                    SCindex;
        double                TSC;
        int                    TSCindex; 
        Ipp32s                    pdist;
    };



    void RFrame_Init(RFrame *t);    
    NGV_Bool RFrame_Allocate(RFrame *t, Ipp32u width, Ipp32u height, Ipp32u padding, RFrameType type);
    void RFrame_Clear(RFrame *t);
    void RFrame_Free(RFrame *t);
    RFrame* RFrame_Construct();
    RFrame* RFrame_Destroy(RFrame *t);


    NGV_Bool RFrame_Convert_Image(RFrame *t, YuvImage *image);
    void RFrame_Copy(RFrame *t, RFrame *src);

    struct RSubband
    {
        Ipp32u         padding;
        YuvSubband* bands[4];
        YuvSubband  bandLL;
        YuvSubband  padded_band_ll;    
        YuvSubband  bandHL;
        YuvSubband  padded_band_hl;
        YuvSubband  bandLH;
        YuvSubband  padded_band_lh;
        YuvSubband  bandHH;
        YuvSubband  padded_band_hh;

        Ipp32u         widthInTiles;
        Ipp32u         heightInTiles;
        Ipp32u            heightInRegionGrid;
        Ipp32u            widthInRegionGrid;

        void        *allocated_buffer;
        Ipp32u         allocated_buffer_size;     

        Ipp32u         y_pitch;
        Ipp32u         u_pitch;
        Ipp32u         v_pitch;

        RSubband *past_frame;
        RSubband *futr_frame;

        RSubband *prev_frame;
        RSubband *next_frame;

        // Extra information
        // each frame contains
        // 1. Pel accurate Shape Mask for Front and Background plate at lowest scale.
        //     a. A Shape byte buffer of image size.
        //     b. Shape contour  lists of all shapes in the frontplate.    
        //     Note: in input RFrame the shape is original and the contour is continous.
        //           in recon RFrame the shape is polygon approximation and the contour is disjoint.
        // 2. A 4x4 Slice Mask and Edge Mask
        //      a. -1 = not in picture or picture edge
        //      b. 0-N = slice number
        // 3. Each RFrame is pel extended to 4x4 (Luma) using the shape mask.
        // 4. Each RFrame is then padded upto +-16 pels for Motion search.
        // 5. Scalable Shape update is available at ful res.
        Ipp32u                 ePicType;
        Ipp32u                 TR;
        NGV_Bool             coded;
        NGV_Bool             displayed;
        NGV_Bool             reference;
    } ;


    /**
    * An encapsulation / abstraction for interger source and destinations 
    * when 1 level decomposition is used.
    */

    void RSubband_Init(RSubband *t);

    NGV_Bool RSubband_Allocate(RSubband *t, Ipp32u width, Ipp32u height, Ipp32u padding);
    void RSubband_Clear(RSubband *t);
    void RSubband_Free(RSubband *t);

    RSubband * RSubband_Construct();
    RSubband * RSubband_Destroy(RSubband *t);

    //---------------------------------------------------------

    /* The RFrameVector class implements a doubly-linked list of */
    /* DecodedFrame objects.  It uses the past_frame and next_frame */
    /* members of the RFrame class to implement the links. */


    struct RFrameVector
    {
        RFrame         *head;
        RFrame         *tail;
        /* head points to the first element in the list and tail */
        /* points to the last.  */

        Ipp32u             length;
    } ;

    RFrameVector * RFrameVector_Construct();
    void RFrameVector_Destroy(RFrameVector *list);

    void RFrameVector_Init(RFrameVector *t); 
    void RFrameVector_Delete(RFrameVector *t);
    RFrame* RFrameVector_Detach_Head(RFrameVector *t);
    void RFrameVector_Append_Tail(RFrameVector *t,  RFrame *pFrame);
    void RFrameVector_Append_Before_Tail(RFrameVector *t,  RFrame *pFrame);
    void RFrameVector_Remove(RFrameVector *t,  RFrame *pFrame);
    void RFrameVector_Insert_List(RFrameVector *t, RFrameVector *src);

    /* The RFrameVector class implements a doubly-linked list of */
    /* DecodedFrame objects.  It uses the past_rame and futr_Frame */
    /* members of the RFrame class to implement the links. */


    struct RSubbandVector
    {
        RSubband         *head;
        RSubband         *tail;
        /* head points to the first element in the list and tail */
        /* points to the last.  */

        Ipp32u             length;

    } ;

    RSubbandVector * RSubbandVector_Construct();
    void RSubbandVector_Destroy(RSubbandVector *list);

    void RSubbandVector_Init(RSubbandVector *t); 
    void RSubbandVector_Delete(RSubbandVector *t);
    RSubband* RSubbandVector_Detach_Head(RSubbandVector *t);
    void RSubbandVector_Append_Tail(RSubbandVector *t,  RSubband *pBands);
    void RSubbandVector_Append_Before_Tail(RSubbandVector *t,  RSubband *pBands);
    void RSubbandVector_Remove(RSubbandVector *t,  RSubband *pBands);
    void RSubbandVector_Insert_List(RSubbandVector *t, RSubbandVector *src);


    /* The RFrameQueue class implements a doubly-linked list of */
    /* Frame objects.  It uses the prev_frame and next_frame */
    /* members of the RFrame class to implement the links. */


    struct RFrameQueue
    {
        RFrame         *head;
        RFrame         *tail;
        /* head points to the first element in the list and tail */
        /* points to the last.  */

        Ipp32u             length;

    } ;

    RFrameQueue * RFrameQueue_Construct();
    void RFrameQueue_Destroy(RFrameQueue *list);

    void RFrameQueue_Init(RFrameQueue *t); 
    void RFrameQueue_Delete(RFrameQueue *t);
    RFrame* RFrameQueue_Detach_Head(RFrameQueue *t);
    void RFrameQueue_Append_Tail(RFrameQueue *t,  RFrame *pFrame);
    void RFrameQueue_Remove(RFrameQueue *t,  RFrame *pFrame);
    void RFrameQueue_Insert_List(RFrameQueue *t, RFrameQueue *src);

    /* The RSubbandQueue class implements a doubly-linked list of */
    /* DecodedFrame objects.  It uses the prev_frame and next_frame */
    /* members of the RSubband class to implement the links. */


    struct RSubbandQueue
    {
        RSubband         *head;
        RSubband         *tail;
        /* head points to the first element in the list and tail */
        /* points to the last.  */

        Ipp32u             length;
    } ;

    RSubbandQueue * RSubbandQueue_Construct();
    void RSubbandQueue_Destroy(RSubbandQueue *list);

    void RSubbandQueue_Init(RSubbandQueue *t); 
    void RSubbandQueue_Delete(RSubbandQueue *t);
    RSubband* RSubbandQueue_Detach_Head(RSubbandQueue *t);
    void RSubbandQueue_Append_Tail(RSubbandQueue *t,  RSubband *pBands);
    void RSubbandQueue_Remove(RSubbandQueue *t,  RSubband *pBands);
    void RSubbandQueue_Insert_List(RSubbandQueue *t, RSubbandQueue *src);

    struct ImDetails        {
        Ipp32u                        width;
        Ipp32u                        height;
        Ipp32u                        exwidth;
        Ipp32u                        exheight;
        Ipp32u                        sidesize;
        Ipp32u                        initPoint;
        Ipp32u                        endPoint;
        Ipp32u                        fBlocks;
        Ipp32u                        wBlocks;
        Ipp32u                        hBlocks;
        Ipp32u                        MVspaceSize;
        Ipp32u                        Yframe;
        Ipp32u                        YUVframe;
        Ipp32u                        UVframe;
        Ipp32u                        exYframe;
        Ipp32u                        block_size_w;
        Ipp32u                        block_size_h;
    };

    struct VidData    {
        ImDetails                R1;
        ImDetails                R4er;
        ImDetails                R16th;
        char*                    inputFile;
        char*                    outputFile;
        char*                    mvFile;
        char*                    SADfile;
        char*                    shotFile;
        char*                    pdistFile;
        Ipp32u                        accuracy;
        Ipp32u                        frameRange;
        Ipp32u                        NumOfFrames;
        Ipp32u                        jumpToFrame;
        Ipp32u                        filter;
        Ipp32u                        stages;
        Ipp32s                        minpdist;
        Ipp32s                        maxpdist;
        Ipp32s                        varpdist;
        Ipp32s                     pdistCorrect;
        Ipp32s                     bForcePDist;
        Ipp32u                     uForcePDistVal;
        Ipp32s                     bNoSceneDetect;
        Ipp32u                        fps;
        Ipp32u                        Key;
        Ipp32s                        logLevel; 
        Ipp32s                     uRefMod;
    };

    struct YUV        {
        Ipp8u*                        Y;
        Ipp8u*                        U;
        Ipp8u*                        V;
    };

    struct MVector            {
        Ipp32s                        x;
        Ipp32s                        y;
    };

    //struct FILE;



    struct Rsad            {
        Ipp32u                        SAD;
        Ipp32u                        distance;
        MVector                    BestMV;
        Ipp32s                        angle;
        Ipp32u                        direction;
    };

    struct imageData    {
        YUV                        exImage;
        YUV                        Backward;
        YUV                        Pframe;
        YUV                        deltaB;
        MVector                    *pBackward;
        MVector                    *pBhalf;
        MVector                    *pBquarter;
        YUV                        deltaF;
#ifndef LESS_MEM_PDIST
        MVector                    *pForward;
        MVector                    *pFhalf;
        MVector                    *pFquarter;
        YUV                        Forward;
        YUV                        Average;
        Ipp32u                        *FSAD;
        Ipp32u                        fSval;
        Ipp32u                        *ASAD;
        Ipp32u                        aSval;
#endif
        Ipp32u                        *BSAD;
        Ipp32u                        bSval;
        Ipp64f*                    Cs;
        Ipp64f*                    Rs;
        Ipp64f                        CsVal;
        Ipp64f                        RsVal;
        Ipp64f*                    DiffCs;
        Ipp64f*                    DiffRs;
        Ipp64f                        DiffCsVal;
        Ipp64f                        DiffRsVal;
    };

    //struct RFrame;

    struct     TSCstat {
        Ipp32u                        frameNum;
        Ipp64f                        Rs;
        Ipp64f                        Cs;
        Ipp64f                        SC;
        Ipp64f                        AFD;
        Ipp64f                        TSC;
        Ipp32s                        SCindex;
        Ipp32s                        TSCindex;
        Ipp32s                        pdist;
        Ipp32s                        Schg;
        NGV_Bool                    Gchg;
        Ipp8u                        picType;
        NGV_Bool                    repeatedFrame;
        RFrame                    *ptr; 
    };

    struct VidSample    {
        imageData                R1Buffer;
        imageData                R4erBuffer;
        imageData                R16thBuffer;
        //YUV                        Pframe;
    } ;

    struct VidRead    {
        VidSample               sample;
        // Temp QPEL Buffers
        YUV                        spBuffer[8];
        // Temp HPEL Buffers
        YUV                        cornerBox;
        YUV                        topBox;
        YUV                        leftBox;
        // Temp Filt Buffers
        Ipp8u*                        bufferH;
        Ipp8u*                        bufferQ;
        double                    average;
        double                    avgSAD;
        Ipp32u                        longerMV;
        TSCstat                    logic[MAXPDIST+1+MAX_LOOKAHEAD];
        Ipp32s*                    PDistanceTable;
    };

#include <stdio.h>
    struct DataWriter            {
        char*                    filename;
        FILE*                    pDataOut;
    };

    void     VidSampleInit(VidSample *videoIn);
    NGV_Bool VidSampleAlloc(void *vt, VidSample *videoIn, VidData *inData);
    NGV_Bool VidSampleFree(void *vt, VidSample *videoIn);

    void    readerInit(VidRead *videoIn);
    void    writerInit(DataWriter *dataOut);

    NGV_Bool    PdYuvFileReader_Open(VidRead *videoIn, VidData *inData);
    NGV_Bool    PdYuvFileReader_Free(VidRead *videoIn);

    void    RsCsCalc(imageData *exBuffer, ImDetails vidCar);
    void    RsCsCalc2(imageData *exBuffer, ImDetails vidCar);
    void    deltaIm(Ipp8u* dd, Ipp8u* ss1, Ipp8u* ss2, Ipp32u widthd, Ipp32u widths, ImDetails ImRes);
    void    deltaIm2(Ipp8u* dd, Ipp8u* ss1, Ipp8u* ss2, Ipp32u widthd, Ipp32u widths, ImDetails ImRes);

    //*****************************************************
    // NGV PDIST
    //*****************************************************

#if USE_PDMV==0
#define PD_LOCAL_POOL
#endif

#define REFMOD_SHRT_GOP 3
#define REFMOD_LONG_GOP 7
#define REFMOD_MAX      7


    struct NGV_PDist
    {
        int minpdist;
        int maxpdist;
        int pdistCorrect;
        int fps;
        int TR; // Frame Preprocessing Available for this TR
        int unmodTR;
        // circular buffer
        int PdIndex;
        int PdIndexL; //to tackle latency
        // results
        NGV_Bool IPframeFound;
        int IPFrame;
        int remaining;

        void *inData;
        void *video;
        void *SADOut;

        NGV_Bool    bClosedGOP;
        NGV_Bool    bClosedGOPSet;
        NGV_Bool    bForcePDist;
        Ipp32u         uForcePDistVal;
        NGV_Bool    bNoSceneDetect;
        Ipp32u         bRankOrder;
        Ipp32u         bModToF;
        NGV_Bool    bAutoModToF;
        Ipp32u         uRankToF;
        NGV_Bool    bPartialInOrder;
        Ipp32u         uRefMod;
        Ipp32u         uRefModStart;   // Partial Pyramid
        NGV_Bool    bRefModSet;
        Ipp32u         uRefRankMax;
#ifdef PD_LOCAL_POOL 
        void        *samplePool[291];    // MAX_PDIST*8+1+MAX_LOOKAHEAD
        int         samplePoolSize;
        int         samplePoolIndex;
#endif
        Ipp32u         totalFrames; 
        double      avgSC;
        double      avgTSC;
        float       avgSCid; 
        float       avgTSCid; 
        int         lookahead;

        void *videoIn;
        int alloc_pdist;
        int dealloc_pdist;
    } ;

    void NGV_PDist_Init(NGV_PDist *t);

    NGV_Bool NGV_PDist_Alloc(NGV_PDist *t, int width, int height);

    void NGV_PDist_Free(NGV_PDist *t, RFrame *ptr, int length);

    void NGV_PDist_SetMinPDist(NGV_PDist *t, int uLatency);

    void NGV_PDME_Frame(NGV_PDist *t, RFrame *in, RFrame *ref, Ipp32u *PDSAD, T_ECORE_MV *PDMV);

    NGV_Bool NGV_PDist_Determine(NGV_PDist *t,
        RFrame *in,
        NGV_Bool firstFrame,
        NGV_Bool keyFrame,
        NGV_Bool lastFrame
        );

    void PPicDetermineQpMap(NGV_PDist *np, RFrame *inFrame, RFrame *past_frame);

    void IPicDetermineQpMap(NGV_PDist *np, RFrame *inFrame);


    //*****************************************************
    //  QPMAP
    //*****************************************************
    /// Local image characteristics for CUs on a specific depth
    class TQPMap
    {
    private:
        Ipp32u                   m_uiPartWidth;
        Ipp32u                   m_uiPartHeight;
        //Ipp32u                   m_uiNumPartInWidth;
        //Ipp32u                   m_uiNumPartInHeight;
        int                    m_iOffsetLow;
        int                    m_iOffsetHigh;
        //int*                   m_aiQPLevel;
        Ipp64f                 m_dAvgGopSADpp;
        Ipp64f                 m_dAvgGopSCpp;
        Ipp64f                 m_dAvgQP;
    public:
        TQPMap();
        virtual ~TQPMap();

        void  create( int iWidth, int iHeight, Ipp32u uiPartWidth, Ipp32u uiPartHeight );
        void  destroy();

        int*                   m_aiQPLevel;
        Ipp32u                   m_uiNumPartInWidth;
        Ipp32u                   m_uiNumPartInHeight;

        Ipp32u                   getPartWidth()        { return m_uiPartWidth;       }
        Ipp32u                   getPartHeight()       { return m_uiPartHeight;      }
        Ipp32u                   getNumPartInWidth()   { return m_uiNumPartInWidth;  }
        Ipp32u                   getNumPartInHeight()  { return m_uiNumPartInHeight; }
        Ipp32u                   getPartStride()       { return m_uiNumPartInWidth;  }
        void                   setAvgGopSADpp(Ipp64f SADpp) { m_dAvgGopSADpp = SADpp; }
        void                   setAvgGopSCpp(Ipp64f SCpp)   { m_dAvgGopSCpp = SCpp;   }
        void                   setAvgQP(Ipp64f dAvgQP)      { m_dAvgQP = dAvgQP;      }
        Ipp64f                 getAvgGopSADpp()             { return m_dAvgGopSADpp;  }
        Ipp64f                 getAvgGopSCpp()              { return m_dAvgGopSCpp;   }
        Ipp64f                 getAvgQP()                   { return m_dAvgQP;        }
        void                   setOffsetLow(int iLow)       { m_iOffsetLow = iLow;    }
        void                   setOffsetHigh(int iHigh)     { m_iOffsetHigh = iHigh;  }
        void                   setLevel(Ipp32u uiRow, Ipp32u uiCol, int iLevel)   { m_aiQPLevel[uiRow*m_uiNumPartInWidth+uiCol] = iLevel;}

        int                    getQPOffset(Ipp32u uiRow, Ipp32u uiCol)  
        { 
            return (m_aiQPLevel[uiRow*m_uiNumPartInWidth+uiCol]<0)?(m_aiQPLevel[uiRow*m_uiNumPartInWidth+uiCol]*m_iOffsetLow):((m_aiQPLevel[uiRow*m_uiNumPartInWidth+uiCol]>0)?(m_aiQPLevel[uiRow*m_uiNumPartInWidth+uiCol]*m_iOffsetHigh):0); 
        }

        int                    getDQP(int ctb)
        {
            VM_ASSERT(ctb >= 0 && ctb < (int)(m_uiNumPartInWidth*m_uiNumPartInHeight));
            if(ctb < 0 || ctb >= (int)(m_uiNumPartInWidth*m_uiNumPartInHeight)) ctb = 0;
            return m_aiQPLevel[ctb];
        }

    };
    //#endif

    //*****************************************************
    // Source Video analyzer class
    //*****************************************************
    class TVideoPreanalyzer
    {
        Ipp8u* m_pchFile;
        int   m_iWidth;
        int   m_iHeight;
        int   m_fileBitDepthY; ///< bitdepth of input/output video file luma component
        int   m_fileBitDepthC; ///< bitdepth of input/output video file chroma component
        int   m_iFrameRate;
        int   m_iGOPSize;
        Ipp32u  m_uiIntraPeriod;
        Ipp32u  m_uiSkipFrames;
        Ipp32u  m_uiFramesToBeEncoded;

    public:
        TVideoPreanalyzer() {}
        virtual ~TVideoPreanalyzer() {}

        void  open  ( Ipp8u* pchFile, Ipp32u width, Ipp32u height, int fileBitDepthY, int fileBitDepthC, int iFrameRate, int iGOPSize, Ipp32u uiIntraPeriod, Ipp32u framesToBeEncoded); ///< open file, Alloc Analyzer
        void  skipFrames(Ipp32u numFrames);
        bool  preAnalyzeOne(TQPMap*   acQPMap);  
        void  close ();                                           ///< close file, Release Analyzer

        // interface part
        mfxU32           m_stagesToGo;

        AsyncRoutineEmulator m_emulatorForSyncPart;
        AsyncRoutineEmulator m_emulatorForAsyncPart;

        // data
        NGV_PDist*   m_np;
        RFrameVector m_InputFrameList;
        RFrameQueue  m_InputFrameQueue;
        RFrameVector *m_pInputFrameList;
        RFrameQueue  *m_pInputFrameQueue;
        
        int m_corrected_height;
        int m_corrected_width;

        FILE* m_dqpFile;
        mfxFrameSurface1*  m_inputSurface;
        int m_uLatency;

        TQPMap*   m_acQPMap;                                        ///< QP Offsets and array of QP Map values
        int m_frameNumber;
        NGV_Bool        m_firstFrame;

        int m_histLength;
    };

    // ====================================================================================================================
    // Class definition
    // ====================================================================================================================
    /// supported slice type
   /* enum SliceType
    {
        B_SLICE,
        P_SLICE,
        I_SLICE
    };*/

    //typedef Ipp16s Pel;

    class TAdapQP
    {
    private:

        int*        m_GOPID2Class;
        Ipp32u        m_qpClass;
        Ipp32u        m_picWidthInCU;
        Ipp32u        m_picHeightInCU;
        Ipp32u        m_edgeCUWidth;
        Ipp32u        m_edgeCUHeight;

    public:

        TAdapQP();
        virtual ~TAdapQP();

        Ipp32u        m_picWidth;
        Ipp32u        m_picHeight;

        int            m_qpBuffer[9];
        int            m_cuQP;
        double        m_rscs;
        int            m_rscsClass;
        int            m_picClass;


        int            m_sliceQP;
        EnumSliceType    m_SliceType;
        int            m_GopSize;
        int         m_GOPSize;//aya??? fixme

        int        m_maxCUWidth;
        int        m_maxCUHeight;
        int        m_cuWidth;
        int        m_cuHeight;
        int        m_maxCUSize;
        int        m_numPartition;

        Ipp32u       m_POC;
        int        m_Xcu;
        int        m_Ycu;
        int        m_cuAddr;    

        /// create internal buffers
        void    create( Ipp32u iMaxWidth, Ipp32u iMaxHeight, Ipp32u picWidth, Ipp32u picHeight, int iGOPSize);
        void    destroy();

        void    setQpBuffer_RSCS();
        int     getDeltaQPFromBase(int dQPIdx);
        int     getQPFromLambda(double dLambda);
        // ===========================================================================================
        void    setSliceQP(int QP);            
        void    setClass_RSCS(double dVal);

        void    set_pic_coding_class(int iGOPId);

        // compute statistics
        //double        compute_block_rscs(Ipp8u *pSrcBuf, int width, int height, int stride);

    };

    template <typename PixType>
double compute_block_rscs(PixType *pSrcBuf, int width, int height, int stride);

#define CLAMP_BIAS  (128+128+24) /* Bias in clamping table */
#define CLIP_RANGE  (CLAMP_BIAS + 256 + CLAMP_BIAS)
#define ClampVal(x)  (ClampTbl[CLAMP_BIAS + (x)])

    extern const Ipp8u  ClampTbl[CLIP_RANGE];

    //*****************************************************
    //   ME 4 PAQ
    //*****************************************************

    //---------------------------------------------------------
// me.h
//---------------------------------------------------------

void ME_SUM_cross_8x8_C(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
void ME_SUM_cross_8x8_SSE(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
void ME_SUM_cross_8x8_Intrin_SSE2(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);

void ME_SUM_Line_8x8_C(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
void ME_SUM_Line_8x8_SSE(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
void ME_SUM_Line_8x8_Intrin_SSE2(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);

void ME_SUM_cross_C(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
void ME_SUM_cross_SSE(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
void ME_SUM_cross_Intrin_SSE2(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);

void ME_SUM_Line_C(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
void ME_SUM_Line_SSE(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
void ME_SUM_Line_Intrin_SSE2(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);

//---------------------------------------------------------
// mest_one.h
//---------------------------------------------------------

Ipp32u   ME_One(VidRead *videoIn,  Ipp32u fPos, ImDetails inData, ImDetails outData, imageData *scale, imageData *scaleRef,
                       imageData *outImage, NGV_Bool first, DIR fdir, Ipp32u accuracy, Ipp32u thres, Ipp32u pdist);

Ipp32u   ME_One3(VidRead *videoIn, Ipp32u fPos, ImDetails inData, ImDetails outData, imageData *scale, imageData *scaleRef,
                       imageData *outImage, NGV_Bool first, DIR fdir, Ipp32u accuracy);

void    calcLimits(Ipp32u xLoc, Ipp32u yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup,
                   Ipp32s *limitYdown, Ipp8u *objFrame, Ipp8u *refFrame, ImDetails inData, Ipp32u fPos);

void    calcLimits2(Ipp32u xLoc, Ipp32u yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup,
                   Ipp32s *limitYdown, Ipp8u *objFrame, Ipp8u *refFrame, ImDetails inData, MVector *MV, Ipp32u fPos);

void    calcLimits3(Ipp32u xLoc, Ipp32u yLoc, ImDetails inData, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup, Ipp32s *limitYdown,
                    Ipp8u *objFrame, Ipp8u *refFrame, MVector *MV, Ipp32u fPos);

void    correctMVector(MVector *MV, Ipp32u xLoc, Ipp32u yLoc, ImDetails inData);
void    correctMVectorOpt(MVector *MV, Ipp32s xpos, Ipp32s ypos, ImDetails inData);

NGV_Bool    searchMV(MVector MV, Ipp8u* curY, Ipp8u* refY, Ipp32u blockSize, Ipp32u fPos, Ipp32u xLoc, Ipp32u yLoc, Ipp32u *bestSAD,
                 Ipp32u *distance, Ipp32u exWidth, Ipp32u *preSAD);

NGV_Bool    searchMV2(MVector MV, Ipp8u* curY, Ipp8u* refY, ImDetails inData, Ipp32u fPos,
        Ipp32u xLoc, Ipp32u yLoc, Ipp32u *bestSAD, Ipp32u *distance, Ipp32u *preSAD);

void    halfpixel(MVector *MV, Ipp32u *bestSAD, Ipp8u* curY, Ipp8u* refY, Ipp32u blockSize, Ipp32u fPos, Ipp32u wBlocks, Ipp32u xLoc,
                  Ipp32u yLoc, Ipp32u offset, VidRead *videoIn, Ipp8u **pRslt, Ipp32u *Rstride, Ipp32u exWidth,
                  NGV_Bool q, MVector *stepq, Ipp32u accuracy, Ipp64f *Rs, Ipp64f *Cs);

void    quarterpixel(VidRead *videoIn, Ipp32u blockSize, MVector *MV, MVector tMV, Ipp32u offset, Ipp8u* fCur, Ipp8u* fRef,
                     Ipp32u *bestSAD, Ipp8u **pRslt, Ipp32u *Rstride, Ipp32u exWidth);

void    corner(Ipp8u*    dd, Ipp8u* ss, Ipp32u exWidth, Ipp32u blockSize);

void    top(Ipp8u* dd, Ipp8u* ss1, Ipp32u exWidth, Ipp32u blockSize, Ipp32u filter);

void    left(Ipp8u* dd, Ipp8u* ss1, Ipp32u exWidth, Ipp32u blockSize, Ipp32u filter);

Ipp32u        SAD_Frame(Ipp8u *Yref1,Ipp8u *Yref2, Ipp32u initPoint, Ipp32u hBlocks, Ipp32u wBlocks, Ipp32u stride, Ipp32u blockSize);

//void __cdecl    ME_One4(VidRead *videoIn, Ipp32u fPos, VidData *inData, NGV_Bool isPframe,
//                        MVector *curMV,Ipp32u *SAD, Ipp8u *objFrame, Ipp8u *refFrame, Ipp8u **result,
//                        Ipp8u *bigFrame, Ipp8u **bigResult, MVector *halfMV, Ipp32u initPoint,
//                        Ipp32u fullinitPoint, Ipp32u exWidth, Ipp32u exHeight, Ipp32u fullexwidth, MVector *steph,
//                        MVector *stepq, NGV_Bool first);

Ipp32u        averageFrame(imageData *ImStore, Ipp8u* forward, Ipp8u* backward, ImDetails inData, Ipp32u frame);
Ipp32u        diffSAD(Ipp8u* forward, Ipp8u* backward, ImDetails inData);
Ipp32u        build(imageData SrcBuffer, ImDetails inData, Ipp32u Tile);
Ipp32u        build3(imageData SrcBuffer, ImDetails inData, Ipp32u Tile);

Ipp32u        searchMV3(MVector Nmv, Ipp8u* curY, Ipp8u* refY, Ipp32u exWidth1, Ipp32u exWidth2);

Ipp32u        halfpel(MVector suBmv, Ipp8u* curY, Ipp8u* refY, Ipp32u blockSize, VidRead *videoIn, double cFactor,
                Ipp32s accuracy, Ipp32u exWidth1, Ipp32u exWidth2, Ipp32u fPos, MVector *pFquarter);
Ipp32u        qpel(VidRead *videoIn, Ipp32u blockSize, MVector Qmv, MVector Nmv, Ipp8u* fCur, Ipp8u* fRef, Ipp32u exWidth1,
             Ipp32u exWidth2);
void    histoMagDir(MVector *MV,Ipp32u fBlocks,Ipp32s **histogram);

Ipp64f        Dist(MVector vector);

//---------------------------------------------------------
// mestproc.h
//---------------------------------------------------------

void    SceneDetect(imageData Data, imageData DataRef, ImDetails imageInfo, Ipp32s scVal, Ipp64f TSC, Ipp64f AFD, NGV_Bool *Schange, NGV_Bool *Gchange);
NGV_Bool    ShotDetect(TSCstat *preAnalysis, Ipp32s position);
//void    printStats(DataWriter *SADOut, TSCstat *data, Ipp32s fps);

//---------------------------------------------------------
//    SAD
//---------------------------------------------------------

Ipp32u SAD_8x8_Block_SSE(Ipp8u* src, Ipp8u* ref, Ipp32u sPitch, Ipp32u rPitch);
Ipp32u SAD_8x8_Block_Intrin_SSE2(Ipp8u* src, Ipp8u* ref, Ipp32u sPitch, Ipp32u rPitch);

#if defined(WIN32)
#ifndef WIN64
#define COMPILE_ASM_CODE
#endif
#endif

} // namespace

#endif // __MFX_H265_PAQ_H__
#else // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_H265_PAQ)

// fake redefenition to reduce macros in source code
typedef void TAdapQP;

#endif
/* EOF */
