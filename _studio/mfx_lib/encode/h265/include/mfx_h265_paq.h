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
    //   PAQ/CALQ UTILS
    //*****************************************************

#define NgvPrintf

#define NgvMalloc(bytes) malloc(bytes)
#define NgvCalloc(n, bytes) calloc(n, bytes)

#define NgvMalloc_aligned(bytes, alignment) ippMalloc(bytes)
#define NgvFree_aligned(ptr) ippFree(ptr)

#define NOTREFERENCED_PARAMETER(P) (P)


#define MAX_TILE_SIZE    64
#define MB_WIDTH         64
#define REGION_GRID_SIZE 16

#define USE_PDMV         1

#undef  NULL
#define NULL             0

#define BLOCK_SIZE       4
#define SEARCH_RANGE     16
#define MBLOCK_SIZE      8
#define LN2              0.6931471805599453
#define INT_MAX          2147483647

#define PI               3.14159265
#define MAXERROR         65025.0
#define MAXPDIST         5
#define RsCsSIZE         16
#define PD_LIMIT         5000000
#define NumTSC           10
#define NumSC            10
#define IFRAME            0
#define PFRAME            1
#define FFRAME            2
#define MAX_LOOKAHEAD     250

#define LESS_MEM_PDIST

#define MAX_RANK_BITS      2
#define MAX_RANK           ((1<<MAX_RANK_BITS)-1)


#define REFMOD_SHRT_GOP    3
#define REFMOD_LONG_GOP    7
#define REFMOD_MAX         7

#define INPUT_IMAGE_QUEUE_SIZE MAXPDIST*(REFMOD_MAX+1)+1

#define CLAMP_BIAS  (128+128+24) /* Bias in clamping table */
#define CLIP_RANGE  (CLAMP_BIAS + 256 + CLAMP_BIAS)
#define ClampVal(x)  (ClampTbl[CLAMP_BIAS + (x)])

    typedef bool  NGV_Bool;

    struct T_ECORE_MV
    {
        Ipp16s     iMVx;
        Ipp16s     iMVy;
    };

    struct MVector 
    {
        Ipp32s     x;
        Ipp32s     y;
    };

    struct YuvImage
    {
        Ipp32u     width;
        Ipp32u     height;
        Ipp32u     size;
        Ipp8u*     y_plane;
        Ipp32u     y_pitch;
    };
    void YuvImage_Init(YuvImage *t);
    void YuvImage_SetAllPointers(YuvImage* t, Ipp8u* y_plane, Ipp32u width, Ipp32u height, Ipp32u y_pitch);

    enum RFrameType
    {
        RFrame_Image,
        RFrame_Recon,
        RFrame_Input,
        RFrame_Types
    };
    
    struct RFrame
    {
        void                *allocated_buffer;
        Ipp32u               allocated_buffer_size;

        YuvImage            image;    
        YuvImage            padded_image;    

        // extra information which makes this rv image a reference frame
        Ipp32u              padding;
        Ipp32u              owidth; // original 
        Ipp32u              oheight;
        Ipp32u              heightInTiles;
        Ipp32u              widthInTiles;
        Ipp32u              heightInRegionGrid;
        Ipp32u              widthInRegionGrid;

        // used with the vector (temporal properties)
        RFrame              *past_frame;
        RFrame              *futr_frame;

        // used with the queue (non temporal properties)
        RFrame              *prev_frame;
        RFrame              *next_frame;

        Ipp32u              ePicType;
        Ipp32u              uPicRank;   // Normative
        Ipp32u              uPicMod;    // Informational
        Ipp32u              TR;
        Ipp32u              npTR;
        NGV_Bool            coded;
        NGV_Bool            reference;
        NGV_Bool            latency; //To control lookahead

        // Preprocessing Data
        RFrameType          type;   // IMAGE, RECON IMAGE, ENCODER INPUT IMAGE
        void                *sample;
        Ipp32u              *BSAD;
        Ipp32u              *PSAD;
        T_ECORE_MV          *PDMV;
        Ipp32u              *PDSAD;
        T_ECORE_MV          *PMV;
        Ipp32u              *FDSAD;
        T_ECORE_MV          *FMV;
        Ipp32s              *qp_mask;
        Ipp32s              *coloc_past;
        Ipp32s              *coloc_futr;
        Ipp32s              *sc_mask;
        Ipp32s              deltaQpLow;
        Ipp32s              deltaQpHigh;

        Ipp32s              Schg;
        double              SC;
        int                 SCindex;
        double              TSC;
        int                 TSCindex;
        Ipp32s              pdist;
    };
    void RFrame_Init(RFrame *t);
    NGV_Bool RFrame_Allocate(RFrame *t, Ipp32u width, Ipp32u height, Ipp32u padding, RFrameType type);
    void RFrame_Clear(RFrame *t);
    void RFrame_Free(RFrame *t);
    RFrame* RFrame_Construct();
    RFrame* RFrame_Destroy(RFrame *t);
    NGV_Bool RFrame_Convert_Image(RFrame *t, YuvImage *image);

    struct RFrameList
    {
        RFrame         *head;
        RFrame         *tail;
        Ipp32u          length;
    };
    void RFrameList_Init(RFrameList *t); 

    typedef RFrameList RFrameVector; // vector used for (temporal properties)
    typedef RFrameList RFrameQueue;  // queue  used for (non temporal properties)

    RFrame* RFrameVector_Detach_Head(RFrameVector *t);
    void RFrameVector_Append_Tail(RFrameVector *t,  RFrame *pFrame);
    
    RFrame* RFrameQueue_Detach_Head(RFrameQueue *t);
    void RFrameQueue_Append_Tail(RFrameQueue *t,  RFrame *pFrame);
    void RFrameQueue_Remove(RFrameQueue *t,  RFrame *pFrame);

    struct ImDetails        
    {
        Ipp32u                   width;
        Ipp32u                   height;
        Ipp32u                   exwidth;
        Ipp32u                   exheight;
        Ipp32u                   sidesize;
        Ipp32u                   initPoint;
        Ipp32u                   endPoint;
        Ipp32u                   fBlocks;
        Ipp32u                   wBlocks;
        Ipp32u                   hBlocks;
        Ipp32u                   MVspaceSize;
        Ipp32u                   Yframe;
        Ipp32u                   exYframe;
        Ipp32u                   block_size_w;
        Ipp32u                   block_size_h;
    };

    struct VidData    
    {
        ImDetails                R1;
        ImDetails                R4er;
        ImDetails                R16th;
        Ipp32u                   accuracy;
        Ipp32u                   filter;
        Ipp32u                   stages;
        Ipp32s                   minpdist;
        Ipp32s                   maxpdist;
        Ipp32s                   varpdist;
        Ipp32s                   pdistCorrect;
        Ipp32s                   bForcePDist;
        Ipp32u                   uForcePDistVal;
        Ipp32s                   bNoSceneDetect;
        Ipp32u                   fps;
        Ipp32u                   Key;
        Ipp32s                   logLevel; 
        Ipp32s                   uRefMod;
    };

    struct YUV 
    {
        Ipp8u*                   Y;
    };

    struct imageData 
    {
        YUV                     exImage;
        YUV                     Backward;
        YUV                     Pframe;
        YUV                     deltaB;
        MVector                 *pBackward;
        MVector                 *pBhalf;
        MVector                 *pBquarter;
        YUV                     deltaF;
        Ipp32u                  *BSAD;
        Ipp32u                  bSval;
        Ipp64f*                 Cs;
        Ipp64f*                 Rs;
        Ipp64f                  CsVal;
        Ipp64f                  RsVal;
        Ipp64f*                 DiffCs;
        Ipp64f*                 DiffRs;
        Ipp64f                  DiffCsVal;
        Ipp64f                  DiffRsVal;
    };

    struct TSCstat 
    {
        Ipp32u                  frameNum;
        Ipp64f                  Rs;
        Ipp64f                  Cs;
        Ipp64f                  SC;
        Ipp64f                  AFD;
        Ipp64f                  TSC;
        Ipp32s                  SCindex;
        Ipp32s                  TSCindex;
        Ipp32s                  pdist;
        Ipp32s                  Schg;
        NGV_Bool                Gchg;
        Ipp8u                   picType;
        NGV_Bool                repeatedFrame;
        RFrame                  *ptr; 
    };

    struct VidSample
    {
        imageData               R1Buffer;
        imageData               R4erBuffer;
        imageData               R16thBuffer;
    } ;

    struct VidRead
    {
        VidSample               sample;
        // Temp QPEL Buffers
        YUV                     spBuffer[8];
        // Temp HPEL Buffers
        YUV                     cornerBox;
        YUV                     topBox;
        YUV                     leftBox;
        // Temp Filt Buffers
        Ipp8u*                  bufferH;
        Ipp8u*                  bufferQ;
        double                  average;
        double                  avgSAD;
        Ipp32u                  longerMV;
        TSCstat                 logic[MAXPDIST+1+MAX_LOOKAHEAD];
        Ipp32s*                 PDistanceTable;
    };

    void     VidSampleInit(VidSample *videoIn);
    NGV_Bool VidSampleAlloc(void *vt, VidSample *videoIn, VidData *inData);
    NGV_Bool VidSampleFree(void *vt, VidSample *videoIn);

    void     readerInit(VidRead *videoIn);

    NGV_Bool PdYuvFileReader_Open(VidRead *videoIn, VidData *inData);
    NGV_Bool PdYuvFileReader_Free(VidRead *videoIn);


    //*****************************************************
    //   PAQ/CALQ CORE STRUCTURES
    //*****************************************************

    // helper for PAQ technique
    class TQPMap
    {
    public:
        TQPMap();
        virtual ~TQPMap();

        void       Init( int iWidth, int iHeight, Ipp32u uiPartWidth, Ipp32u uiPartHeight );
        void       Close();

        int*       m_aiQPLevel;
        Ipp32u     m_uiNumPartInWidth;
        Ipp32u     m_uiNumPartInHeight;

        Ipp32u     getPartWidth()        { return m_uiPartWidth;       }
        Ipp32u     getPartHeight()       { return m_uiPartHeight;      }
        void       setAvgGopSADpp(Ipp64f SADpp) { m_dAvgGopSADpp = SADpp; }
        void       setAvgGopSCpp(Ipp64f SCpp)   { m_dAvgGopSCpp = SCpp;   }
        void       setAvgQP(Ipp64f dAvgQP)      { m_dAvgQP = dAvgQP;      }
        Ipp64f     getAvgGopSADpp()             { return m_dAvgGopSADpp;  }
        Ipp64f     getAvgGopSCpp()              { return m_dAvgGopSCpp;   }
        Ipp64f     getAvgQP()                   { return m_dAvgQP;        }
        void       setOffsetLow(int iLow)       { m_iOffsetLow = iLow;    }
        void       setOffsetHigh(int iHigh)     { m_iOffsetHigh = iHigh;  }
        void       setLevel(Ipp32u uiRow, Ipp32u uiCol, int iLevel)   
        {
            int addr = uiRow*m_uiNumPartInWidth+uiCol;
            int maxLimit = m_uiNumPartInWidth * m_uiNumPartInHeight;
            if(addr >= maxLimit) {
                return; // no problem. ignore is enough 
            }
            m_aiQPLevel[addr] = iLevel;
        }

        int        getDQP(int ctb)
        {
            VM_ASSERT(ctb >= 0 && ctb < (int)(m_uiNumPartInWidth*m_uiNumPartInHeight));
            if(ctb < 0 || ctb >= (int)(m_uiNumPartInWidth*m_uiNumPartInHeight)) ctb = 0;
            return m_aiQPLevel[ctb];
        }

    private:
        Ipp32u                   m_uiPartWidth;
        Ipp32u                   m_uiPartHeight;
        int                    m_iOffsetLow;
        int                    m_iOffsetHigh;
        Ipp64f                 m_dAvgGopSADpp;
        Ipp64f                 m_dAvgGopSCpp;
        Ipp64f                 m_dAvgQP;
    };

    
    // engine for PAQ/CALQ tecniques
    struct NGV_PDist;
    class TVideoPreanalyzer
    {
    public:
        TVideoPreanalyzer() {}
        virtual ~TVideoPreanalyzer() {}

        void  Init( Ipp8u* pchFile, Ipp32u width, Ipp32u height, int fileBitDepthY, int fileBitDepthC, 
            int iFrameRate, int iGOPSize, Ipp32u uiIntraPeriod, Ipp32u framesToBeEncoded, int maxCUSize); 
        void  Close();

        bool  preAnalyzeOne(TQPMap*   acQPMap);

        mfxU32  m_stagesToGo;

        AsyncRoutineEmulator m_emulatorForSyncPart;
        AsyncRoutineEmulator m_emulatorForAsyncPart;
        
        NGV_PDist*   m_np;
        RFrameVector m_InputFrameVector;
        RFrameQueue  m_InputFrameQueue;

        int m_corrected_height;
        int m_corrected_width;

        //FILE* m_dqpFile;
        mfxFrameSurface1*  m_inputSurface;
        int       m_uLatency;

        TQPMap*   m_acQPMap;
        int       m_frameNumber;
        NGV_Bool  m_firstFrame;

        int       m_histLength;

    private:
        Ipp8u*    m_pchFile;
        int       m_iWidth;
        int       m_iHeight;
        int       m_fileBitDepthY; 
        int       m_fileBitDepthC; 
        int       m_iFrameRate;
        int       m_iGOPSize;
        Ipp32u    m_uiIntraPeriod;
    }; 


    // helper for CALQ technique
    class TAdapQP
    {
    public:
        TAdapQP();
        virtual ~TAdapQP();

        Ipp32u  m_picWidth;
        Ipp32u  m_picHeight;
        int     m_maxCUWidth;
        int     m_maxCUHeight;
        int     m_cuWidth;
        int     m_cuHeight;
        int     m_maxCUSize;
        int     m_numPartition;

        int     m_Xcu;
        int     m_Ycu;
        int     m_cuAddr;

        int     m_qpBuffer[9];
        double  m_rscs;
        int     m_rscsClass;
        int     m_picClass;

        int     m_sliceQP;
        Ipp8u   m_SliceType;
        int     m_GOPSize;

        Ipp32u  m_POC;
        
        void    Init( Ipp32u iMaxWidth, Ipp32u iMaxHeight, Ipp32u picWidth, Ipp32u picHeight, int iGOPSize);
        void    Close();

        int     getQPFromLambda(double dLambda);

        void    setSliceQP(int QP);
        void    setClass_RSCS(double dVal);
        void    set_pic_coding_class(int iGOPId);

    private:
        int*    m_GOPID2Class;
        Ipp32u  m_qpClass;
        Ipp32u  m_picWidthInCU;
        Ipp32u  m_picHeightInCU;
    }; // class TAdapQP

    template <typename PixType>
    double compute_block_rscs(PixType *pSrcBuf, int width, int height, int stride);

    extern const Ipp8u  ClampTbl[CLIP_RANGE];

    // ME FOR PAQ
    Ipp32u   ME_One(VidRead *videoIn,  Ipp32u fPos, ImDetails inData, ImDetails outData, imageData *scale, imageData *scaleRef,
        imageData *outImage, NGV_Bool first, int fdir, Ipp32u accuracy, Ipp32u thres, Ipp32u pdist);

    Ipp32u   ME_One3(VidRead *videoIn, Ipp32u fPos, ImDetails inData, ImDetails outData, imageData *scale, imageData *scaleRef,
        imageData *outImage, NGV_Bool first, int fdir, Ipp32u accuracy);

    void     SceneDetect(imageData Data, imageData DataRef, ImDetails imageInfo, Ipp32s scVal, Ipp64f TSC, Ipp64f AFD, NGV_Bool *Schange, NGV_Bool *Gchange);

} // namespace

#endif // __MFX_H265_PAQ_H__
#else // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_H265_PAQ)

// fake redefenition to reduce macros in source code
typedef void TAdapQP;

#endif
/* EOF */
