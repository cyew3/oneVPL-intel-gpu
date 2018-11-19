// Copyright (c) 2007-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <umc_defs.h>
#ifdef UMC_ENABLE_ME

#ifndef _UMC_ME_STRUCTURES_H_
#define _UMC_ME_STRUCTURES_H_

#include <ippi.h>
#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_memory_allocator.h"

#define MAX_REF         32


namespace UMC
{
    //*******************************************************************************************************

        enum{//reference frame direction
            frw=0,
            bkw=1,
            bidir=2
        };
        enum{//YUV plaines index
            Y=0,
            U=1,
            V=2
        };
    //input chroma format: 
    typedef enum
    {
        ME_YUV420=0,
        ME_NV12
    }MeChromaFormat;

    ///Pixel types. It is used to specify subpixel search level, and also used inside ME to choose downsampled level.
    ///It does not define interpolation type. See MeInterpolation.
    typedef enum
    {
        ME_IntegerPixel = 0x1, ///<integer pixel
        ME_HalfPixel    = 0x2, ///<half pixel
        ME_QuarterPixel = 0x4, ///<quarter pixel
        ME_DoublePixel = 0x8, ///< downsample 2 times plane
        ME_QuadPixel = 0x10, ///< 4 times
        ME_OctalPixel = 0x20 ///< 8 times
    } MePixelType;

    ///MB partitioning. Numbering  is important, so don't change it arbitrary!
    ///Unusual types such as 2x2 are used during downsampling search.
    typedef enum
    {
        ME_Mb2x2   = 0x00000001, ///<2x2
        ME_Mb4x4   = 0x00000002, ///<4x4
        ME_Mb8x8   = 0x00000004, ///<8x8
        ME_Mb16x16 = 0x000000008, ///<16x16
        ME_Mb16x8  = 0x000000100, ///<16x8
        ME_Mb8x16  = 0x000000200 ///<8x16
    } MeMbPart;

    ///Interpolation type.
    ///See also MePixelType
    typedef enum
    {
        ME_VC1_Bilinear,
        ME_VC1_Bicubic,
        ME_H264_Luma,
        ME_H264_Chroma,
        ME_AVS_Luma
    } MeInterpolation;

    ///MV prediction type.
    ///See MePredictCalculator.
    typedef enum
    {
        ME_VC1,
        ME_VC1Hybrid,
        ME_VC1Field1,
        ME_VC1Field2,
        ME_VC1Field2Hybrid,
        ME_MPEG2,
        ME_H264,
        ME_AVS
    } MePredictionType;

    ///Search direction.
    ///Used to choose between P and B frames.
    typedef enum
    {
        ME_ForwardSearch = 0,
        ME_BidirSearch,
        ME_IntraSearch
    }MeSearchDirection;

    ///MB type. 
    ///This is standard independent types.
    typedef enum
    {
        ME_MbIntra,
        ME_MbFrw,
        ME_MbBkw,
        ME_MbBidir,
        ME_MbFrwSkipped,
        ME_MbBkwSkipped,
        ME_MbBidirSkipped,
        ME_MbDirectSkipped,
        ME_MbDirect,
        ME_MbMixed
    } MeMbType;

#if 0
    ///Block type. 
    ///This is standard independent types.
    typedef enum
    {
        ME_BlockSkipped,
        ME_BlockFrw,
        ME_BlockBkw,
        ME_BlockBidir,
        ME_BlockIntra,
        ME_BlockOutOfBound
    } MeBlockType;
#endif

    typedef enum
    {
        ME_FrameMc,
        ME_FieldMc
    } MeMcType;

    typedef enum
    {
        ME_FrmIntra,
        ME_FrmFrw,
        ME_FrmBidir
    } MeFrameType;

    ///Different motion estimation paths. See MeBase::DiamondSearch
    ///for detailed path configuration.
    typedef enum
    {
        ME_SmallDiamond, ///<small +-1 point diamond
        ME_MediumDiamond, ///<medium +-2 point diamond
        ME_BigDiamond,///<big +-3 point diamond
        ME_SmallHexagon, ///<small +-8 points 2 level hexagon
        ME_MediumHexagon ///<medium +-16 points 4 level hexagon
    } MeDiamondType;

    ///Different cost metrics used for motion estimation and mode decision.
    ///Not all metrics may be used for motion estimation.
    typedef enum
    {
        ME_Sad, ///< SAD
        ME_Sadt, ///< Hadamard
        ME_SSD, ///< sum of square  differences
        ME_Maxad,
        ME_Hadamard,
        ME_SadtSrc, ///<calculate sum of coefficients of Hadamard transformed src, used for Intra/Inter mode decision
        ME_SadtSrcNoDC, ///<the same, but not taking DC into account
        ME_VarMean,
        ME_InterSSD,
        ME_InterRate,
        ME_IntraSSD,
        ME_IntraRate,
        ME_InterRD,
        ME_IntraRD
    }MeDecisionMetrics;

    typedef enum
    {
        ME_Block0 = 0,
        ME_Block1,
        ME_Block2,
        ME_Block3,
        ME_Macroblock = 10
    }MePredictionIndex;

    ///Different transform types.
    ///Used for standard abstraction.
    typedef enum
    {
        ME_Tranform8x8=0, ///,
        ME_Tranform8x4=1, ///,
        ME_Tranform4x8=2, ///,
        ME_Tranform4x4=3 ///,
    } MeTransformType;

    //*******************************************************************************************************

    typedef struct _MePicRange
    {
        IppiPoint top_left;
        IppiPoint bottom_right;
    }MePicRange;


    typedef struct _MeVC1fieldScaleInfo
    {
        int16_t ME_ScaleOpp;
        int16_t ME_ScaleSame1;
        int16_t ME_ScaleSame2;
        int16_t ME_ScaleZoneX;
        int16_t ME_ScaleZoneY;
        int16_t ME_ZoneOffsetX;
        int16_t ME_ZoneOffsetY;
        int16_t ME_RangeX;
        int16_t ME_RangeY;
        bool   ME_Bottom;

    }MeVC1fieldScaleInfo;

//new: for fields RD-decision(for GetMVSize)
    typedef struct _MeMVFieldInfo
    {
        const uint8_t * ME_pMVSizeOffsetFieldIndexX;
        const uint8_t * ME_pMVSizeOffsetFieldIndexY;
        const int16_t* ME_pMVSizeOffsetFieldX;
        const int16_t* ME_pMVSizeOffsetFieldY;
        const uint32_t* ME_pMVModeField1RefTable_VLC;
        int16_t        ME_limitX;
        int16_t        ME_limitY;
        bool          ME_bExtendedX;
        bool          ME_bExtendedY;
    }MeMVFieldInfo;
    typedef struct
    {
       const uint8_t * pTableDL;
       const uint8_t * pTableDR;
       const uint8_t * pTableInd;
       const uint8_t * pTableDLLast;
       const uint8_t * pTableDRLast;
       const uint8_t * pTableIndLast;
       const uint32_t* pEncTable;
    } MeACTablesSet; //it is ME version of UMC_VC1_ENCODER::sACTablesSet


    //*******************************************************************************************************
    ///Motion vector. 
    ///Always in quarter pixel units.
    class MeMV
    {
    public:
        /** Default constructor */
        MeMV(){};
        ///Create MV with identical x and y component, mostly used in mv=0 sentences 
        ///\param a0 component value
        MeMV(int a0){x = (int16_t)a0; y=(int16_t)a0;};
        ///Create MV with different x and y component
        ///\param a0 x value \param a1 y value
        MeMV(int a0, int a1){x = (int16_t)a0; y=(int16_t)a1;};
        int16_t x;
        int16_t y;

        /**Set invalid value to MV.*/
        void SetInvalid(){x=0x7fff;y=0x7fff;};
        /**Check if MV is invalid.*/
        bool IsInvalid(){return x==0x7fff && y==0x7fff;}; 
        /**Add two MVs.*/
        inline MeMV operator + (const MeMV& a)
        {return MeMV(a.x + x, a.y + y);};
        /**Compare two MVs.*/
        inline bool operator == (const MeMV& a)
        {return a.x==x && a.y==y;};
    };


    ///Class used to represent addresses and steps of point in all three planes Y, U and V.
    ///Also used in downsampling search to reference to different levels.
    class MeAdr
    {
        public:
        /**Constructor */    
        MeAdr(){clear();};
        /**Clear planes pointers.*/
        inline void clear(){chroma=false; channel=0; ptr[0]=ptr[1]=ptr[2]=NULL;};
        /**Set one channel.\param ch channel Y,U or V, \param pt pointer to plane \param st plane step*/
        inline void set(int32_t ch, uint8_t *pt, int32_t st){ptr[ch]=pt; step[ch]=st;};
        /**Move pointers in both x and y direction for given channel\param ch channel index to move \param dx x offset \param dy y offset*/
        inline void move(int32_t ch, int32_t dx, int32_t dy){ptr[ch]+=dy*step[ch]+dx;};
        /**Move pointers in both x and y direction for luma if chroma==false or for all channels. Chroma offset is two times smaller than for luma.
             \param dx x offset \param dy y offset*/
        inline void move(MeChromaFormat f, int32_t dx, int32_t dy)
                {
                    int32_t num;
                    num = (f == ME_YUV420) ? 3 : 2;
                    for(int i=0; i<(chroma?num:1);i++)
                    {
                        move(i,dx,dy);
                        if(i==0){
                            if(f == ME_YUV420)
                                {dx/=2;dy/=2;}
                            else{dy/=2;}
                        }
                    }
                };
        /**Return pointer in given channel offseted by x and y pixel \param ch channel index \param dx x offset \param dy y offset*/
        inline uint8_t* at(int32_t ch, int32_t dx, int32_t dy){return ptr[ch]+dy*step[ch]+dx;};
        /**Operator is used to preserv chroma flag and selected channel during object copying*/        
        void operator=(const MeAdr &a){for(int i=0; i<3; i++){ptr[i]=a.ptr[i];step[i]=a.step[i];}};
            
        bool chroma; ///< true - chroma planes are valid or chroma address should be loaded
        int32_t channel; ///< index of active channel, it is used in some cost calculation
        uint8_t* ptr[3];  ///< plane pointers
        int32_t step[3]; ///< plane steps
    };


    ///Class is used 
    ///to represent RD cost.
    class MeCostRD
    {
        public:
            MeCostRD(){};
            MeCostRD(int32_t x){R=D=J=x;NumOfCoeff=BlkPat=SbPat=0;};
            int32_t R;
            int32_t D;
            int32_t J;
            int32_t NumOfCoeff;
            int32_t BlkPat; //block pattern, bit 0 is 5-th block chroma, bit 5 is 0 block luma
            int32_t SbPat; //sub block pattern, for varible transform. For 8x8 transform only bit 0 is set, for 8x4 and 4x8 bit 1 is for 0 subblock, bit 0 is for 1-st block
            
            inline void operator +=(const MeCostRD& a) {R+=a.R;D+=a.D;NumOfCoeff+=a.NumOfCoeff;BlkPat|=a.BlkPat;SbPat|=a.SbPat;J=0;};
    };

    ///Statistics for one MB. It is used in feedback implementation. All fields are filled in by encoder, except dist.
    ///It is currently filled in by ME, though it is violation of architecture.
    class MeMbStat{
    public:
        uint16_t  whole; ///<whole MB size in bit, including MV, coefficients and any additional info. 
        uint16_t* MVF; ///<size of each MVs in bit, number of allocated element is MeFrame::NumOfMVs
        uint16_t* MVB; ///<size of each MVs in bit, number of allocated element is MeFrame::NumOfMVs
        uint16_t* coeff; /**< \brief size of coefficients in bit, for each block. Number of element is MeFrame::NumOfMVs.
                            Block is motion compensated entity, not the transformation block. So if there is only one MV for MB,
                            then only one element is set here, even if 4x4 transformation was used. It is intended for simplifying
                            feedback implementation.*/
                            // TODO: update usage model!
        uint16_t  qp; ///< QP used for encoding of this MB
        uint8_t  MbType; ///<coded MB type MbIntra, MbFrw, MbBkw, MbBidir..., may be different from MeFrame::MBs::MbType
        uint32_t dist; ///<whole 16x16 MB distortion, MSD?
    };

    ///Class represents motion estimation and mode decision results. Also used to store some intermediate  data
    ///from motion estimation. See MeFrame::MBs.
    class MeMB {
    public:
        void Reset();
        void Reset( int32_t numOfMVs);
        void CnvertCoeffToRoundControl(int16_t *transf, int16_t *trellis, int16_t *rc, int32_t QP);///< function converts trellis quantized coefficients to round control value for each coefficient
        
        MeMV      *MV[2];  ///<best found MV, 0 - forward, 1- backward, order: fwd1, fwd2, fwd3, fwd4, number of allocated element is MeFrame::NumOfMVs
        int32_t    *Refindex[2];///<indexes for the reference frames, 0-forward, 1-backward
        int32_t    *MbCosts; ///<costs of best MVs. number of allocated elements is equal to number MeFrame::NumOfMVs+1
                            // TODO: chroma?
        MeMV      *MVPred[2]; ///<Predicted MV for current MB. 0 - forward, 1- backward; number of allocated elements is equal to number of MVs;
        uint8_t     MbPart; ///<MB partitioning 16x16, 16x8, 8x16, 8x8, 4x4 -> MeMbPart
        uint8_t     BlockPart; ///<NOT USED, block partitioning, 4x2 bits field, 00 - 8x8, 01 - 8x4, 10 - 4x8, 11 - 4x4
        uint8_t     MbType; ///<MbIntra,MbFrw,MbBkw,MbBidir,MbFrw_Skipped,MbBkw_Skipped,MbBidir_Skipped,MbDirect  -> MeMbType
        uint8_t     McType; ///<NOT USED, Motion compensation type, see MeMcType 
        uint8_t     BlockType[4];///<Block type for each of 8x8 blocks. MeMbType is used to simplify 16x16 vs 8x8 comparision.
        int32_t     BlockTrans; ///<NOT USED, block transforming, 6x2 bits field, 00 - 8x8, 01 - 8x4, 10 - 4x8, 11 - 4x4, lowest bits 1:0 for block 0, 2:1 for block 2 and so on 11:10 for block 5
        //uint8_t*    Difference; //NULL - no differences was calculated
        
        // AVS specific
        uint8_t predType[4];     // (uint8_t []) prediction types of each block: only MbIntra,MbFrw,MbBkw,MbBidir

        //VC1 specific, used for FB only
        int32_t  NumOfNZ; //it is number of non zero blocks after transforming and quantisation
        int32_t  PureSAD; //pure inter SAD, there is no MV weighting in it
        MeCostRD InterCostRD; //value for inter rate and distortion prediction
        MeCostRD IntraCostRD; //value for intra rate and distorton prediction
        MeCostRD DirectCostRD; //value for direct inter rate and distortion prediction
        MeCostRD DirectSkippedCostRD; //value for direct skipped inter rate and distortion prediction
        MeCostRD SkippedCostRD; //value for skipped inter rate and distortion prediction
        int32_t  DcCoeff[6]; //DC coefficients
        int16_t Coeff[6][64]; //quantized coefficients
        int8_t RoundControl[6][64]; //quantized coefficients
    };

    ///Class represents one frame. It may be either source or reference frame. 
    ///Used to simplify interface by grouping all frame attributes in one class and to avoid unnecessary downsampling.
    class MeFrame
    {
    public:
        bool    processed; ///<Frame was already processed by ME. It is set by ME, reset by encoder
//        int32_t      index; //set by encoder
        MeFrameType type; ///< Frame type, set by encoder
        bool bBottom;///< is bottom field

        int32_t      WidthMB; ///< Frame width in MB. Set by encoder
        int32_t      HeightMB;///< Frame height in MB. Set by encoder
        int32_t      padding;  /**< \brief number of pixels that was added on each side of the frame, should be less or equal 
                                        to MeInitParams.refPadding and divisible by 16. Zero is also possible.*/
        MeAdr       plane[5]; /**< \brief Image plains. 5 is number of downsampled levels, zero level is allocated by encoder, this is original picture, all other by ME. 
                                Not all levels may be allocated. Pointers on all levels point to the left top pixel of picture without padding*/
        // TODO: add initializaton
        MeAdr       SrcPlane; //< This original, uncompressed image plane. May oy may not be the same as MeFrame::plane[0]. It is used for distortion calculation in feedback.
        MeAdr       RecPlane; //< This reconstructed image plane. May oy may not be the same as MeFrame::plane[0].  It is used for distortion calculation in feedback.

        int32_t      NumOfMVs; ///<num of MVs in MBs structure, it set by ME, should not be changed by encoder
        MeMB*       MBs; ///<Estimation results for current frame. Number of element is equal to number of MB in frame. Allocated by MeBase
        MeMV*       MVDirect;///< Direct motion vectors, calculated and set by encoder. Currently VC1 specific. allocated by MeBase
        int32_t*     RefIndx;///< Reference indexes for direct MVs. Currently VC1 specific. allocated by MeBase
        // TODO: add initializaton
        bool        StatValid; ///< true - encoder filled in stat array for this frame. It is set by encoder and reset by ME after processing
        MeMbStat*   stat;  ///< Statistics for current frame. Filled in by encoder.  Number of element is equal to number of MB in frame. allocated by MeBase
        // AVS specific
        int32_t m_blockDist[2][4];                      // (int32_t [][]) block distances, depending on ref index
        int32_t m_distIdx[2][4];                        // (int32_t [][]) distance indecies of reference pictures
        int32_t picture_distance;
    };

    ///Class used to initialize ME. Amount of allocated memory depend on provided values. 
    ///See MeBase::Init for detailed description of ME initialization.
    class MeInitParams
    {
    public:
        MeInitParams();
        int32_t            MaxNumOfFrame; ///< Maximum number of frame to allocate.
        MeFrame*          pFrames; ///< Allocated frames. Returned by ME.
        
        int32_t            WidthMB; ///<Frame width in MB, used only for memory allocation.
        int32_t            HeightMB;///<Frame height in MB.
        int32_t            refPadding;///<Reference frame padding. Unlike MeFrame::padding may be any value.
        MeSearchDirection SearchDirection;///<forward, bidir
        int32_t            MbPart;///<16x16,16x8,8x16,8x8,4x4
        bool UseStatistics;///<true - allocate memory for statistics, MeFrame::stat
        bool UseDownSampling;///<true - allocate memory for downsampling MeFrame::plane[x]
        bool bNV12;
    };

    ///Class used to specify current frame estimation. All parameters should not
    ///excide value passed to MeBase::Init.
    class MeParams
    {
    public:
        MeParams();
        void SetSearchSpeed(int32_t Speed);

        int32_t   FirstMB;  ///< Address of first MB for estimation. Used for multy threading slice processing.
        int32_t   LastMB;  ///< Address of last MB for estimation. Used for multy threading slice processing.

        MeFrame* pSrc; ///< Pointer to source frame. Estimation results will be returned in pSrc->MBs

        int32_t              FRefFramesNum; ///< Number of forward reference frames
        int32_t              BRefFramesNum; ///< Number of backward reference frames
        MeFrame* pRefF[MAX_REF]; ///< array of forward reference frames, array allocated by ME, frames by encoder
        MeFrame* pRefB[MAX_REF]; ///< the same for backward
        bool     IsSMProfile;

        IppiPoint           SearchRange; ///<This is MV range in pixel. Resulting MV will be from -4*SearchRange.z to 4*SearchRange.z-1
        MeSearchDirection   SearchDirection;///< May be forward or bidir
        MePredictionType    PredictionType;///< One of possible prediction
        MeInterpolation     Interpolation; ///< Interpolation type for luma channel
        MeInterpolation     ChromaInterpolation; ///< Interpolation type for chroma
        int32_t              MbPart;///< List of partitions to check. Only partition from this list will be checked.
        MePixelType         PixelType; ///<Subpixel type to search. 
        MePicRange          PicRange; /**<\brief It is area where ME is allowed relative to left top pixel of the frame. Negative values are possible.
                                All pixels of any block or MB will remain within these borders for any resulting MV.
                                Top and left ranges are inclusive. So if top_left.x=-8, MV.x=-4*8 is smallest possible x value for MB with address 0.
                                Right and bottom are not inclusive. So if bottom_right.x=720, MV.x=4*720-4*16 is biggest possible MV for 16x16 MB 0. */
        MeDecisionMetrics   CostMetric; ///< inter MB metric
        MeDecisionMetrics   SkippedMetrics;///<skip MB metric
        bool                bSecondField;///< true - the current coded picture is the second field
        int32_t              Quant; ///< quantizer for current picture
        bool                UniformQuant; ///< true - uniform quantization will be used. VC1 specific 
        bool                ProcessSkipped; ///< ture - estimate skipped MB
        bool                ProcessDirect; ///< true - estimate direct MB
        bool                FieldMCEnable;  ///< true - field MB mode, NOT USED
        bool                UseVarSizeTransform; ///< true - choose variable size transforming for Inter blocks. VC1 specific 

        bool UseFastInterSearch; ///< true - use fastest possible ME algorithm. Feature leads to significant speed up and to significant quality degradation.
        bool UseDownSampledImageForSearch; /**< \brief true - enable search on downsampled pictures. Feature should be enabled in MeBase::Init. 
                           It leads to significant speed up without noticeable quality degradation. It is recommended to keep it on. Except in memory tight environment.*/
        //bool UseFastIntraInterDecision;
        bool UseFeedback; /**<\brief true - Use feedback from encoder. This feature significantly (if UseFastFeedback==false) improves quality with moderate speed lost.
                                           Implemented only for 16x16 forward mode now.*/
        bool UpdateFeedback; /**<\brief false - use feedback with default presets, no actual feedback from encoder is needed. true - update presets 
                                    by feedback from encoder. Feature leads to quality improvement on some bizarre streams with some speed degradation. */
        bool UseFastFeedback; ///<false - use thorough, transform based, distortion/rate prediction. true - use less acuratre but more fast prediction
       // bool UseChromaForME; ///<true - use chrome component during ME. Feature leads to small quality improvement, with some speed degradation. NOT USED.
        bool UseChromaForMD; ///<true - use chrome component during mode decision. Turn it on to avoid chromatic artifacts.
        bool ChangeInterpPixelType; ///<true - change interpolation and in the end of the search
        bool SelectVlcTables;
        bool UseTrellisQuantization;
        bool UseTrellisQuantizationChroma;

        //VC1 specific
        bool FastChroma; ///< VC1 specific
        MeVC1fieldScaleInfo ScaleInfo[2];///<the structure for field vector predictors calculation
        MeMVFieldInfo  FieldInfo;///<the structure for field MV size calculation (used in GetMVSize)

        //VC1 specific, used for RD mode decision
        float      Bfraction; 
        int32_t      MVRangeIndex;
        const uint8_t*    MVSizeOffset;
        const uint8_t*    MVLengthLong;

        const uint32_t*  DcTableLuma;
        const uint32_t*  DcTableChroma;

        const uint8_t* ScanTable[4]; //8x8 8x4 4x8 4x4 scan tables 

        int32_t CbpcyTableIndex;
        int32_t MvTableIndex;
        int32_t AcTableIndex;

        //output parameters (recommendation for encoder)
        MeInterpolation     OutInterpolation;
        MePixelType         OutPixelType;
        int32_t OutCbpcyTableIndex;
        int32_t OutMvTableIndex;
        int32_t OutAcTableIndex;

//    protected:
        int32_t SearchSpeed; ///< search speed, see SetSearchSpeed
    };


    const int ME_NUM_OF_REF_FRAMES = 2;
    const int ME_NUM_OF_BLOCKS = 4;
    const int ME_BIG_COST = 0x1234567; //19.088.743 it is about 100 times smaller than 2147483647, this is reserve to prevent overflow during calculation


    class MeSearchPoint
    {
        public:
            int32_t cost; 
            int16_t x;
            int16_t y;
    };

    static const int NumOfLevelInTrellis=2;
    class MeTrellisNode {
    public:
        void Reset(){active=true; last = false; level=run=0; cost=0; prev=NULL;};
        
        bool active;
        bool last;
        int32_t level;
        int32_t run;
        int32_t cost;
        int32_t rc;
        MeTrellisNode *prev;
    };
  

    class MeCurrentMB
    {
    public:
        void Reset();
        void ClearMvScoreBoardArr();
        int32_t CheckScoreBoardMv(MePixelType pix,MeMV mv);
        void SetScoreBoardFlag();
        void ClearScoreBoardFlag();

        int32_t x; //MB location relative to left top corner of the frame
        int32_t y;
        int32_t adr; //MB address

        //skip
        int32_t SkipCost[ME_NUM_OF_BLOCKS];
        MeMbType SkipType[ME_NUM_OF_BLOCKS]; //For VC1 - ME_MbFrwSkipped, ME_MbBkwSkipped or ME_MbBidirSkipped
        int32_t SkipIdx[2][ME_NUM_OF_BLOCKS]; //reference frames indexes for skip, second index - frw or bkw

        //direct
        int32_t DirectCost;
        MeMbType DirectType; //For VC1 - ME_MbDirectSkipped, ME_MbDirect
        int32_t DirectIdx[2]; //reference frames indexes for direct

        //inter
        //these variable are used not only for inter type search but also as temporal place to store mode decision results
        //for blocks. This behavior may be changed later.
        int32_t BestCost; //intermediate cost for current MB, block or subblock
        int32_t InterCost[ME_NUM_OF_BLOCKS]; //this is best found cost for frw or bkw or bidir Inter MB or block or subblock
        MeMbType InterType[ME_NUM_OF_BLOCKS]; //this is best found type of current MB, block or subblock
        int32_t BestIdx[2][ME_NUM_OF_BLOCKS]; //frw and bkw frames indexes of best cost
        MeMV BestMV[2][ME_NUM_OF_REF_FRAMES][ME_NUM_OF_BLOCKS]; //all best found MVs
        MeTransformType InterTransf[10][6]; //transformation type; the first index - MB type, the second - block number
        bool HybridPredictor[ME_NUM_OF_REF_FRAMES]; //current MV uses hybrid predictor. It is used during RD cost calculation. VC1 specific

        //intra
        int32_t IntraCost[ME_NUM_OF_BLOCKS]; //Intra cost
        //SomeType IntraType[ME_NUM_OF_BLOCKS]; //intra prediction type for H264

        //place to save calculated residuals
        MeAdr buf[3]; //temporary buffers for iterplolation, index is direction 0 - frw, 1 - bkw, 2 - bidir, all three planes are allocated for each direction

        MeMV PredMV[2][ME_NUM_OF_REF_FRAMES][ME_NUM_OF_BLOCKS]; //MV prediction

        int32_t RefIdx; //reference frame index
        int32_t RefDir; //reference frame direction frw or bkw

        int32_t BlkIdx; //current block idx
        MeMbPart MbPart; //current MB partitioning

        int32_t DwnCost[5];
        int32_t DwnNumOfPoints[5];
        MeSearchPoint DwnPoints[5][16*1024]; // TODO: allocate in init

        //slice level statistics
        int32_t CbpcyRate[4]; //number of bit to encode CBPCY element using each of four possible tables
        int32_t MvRate[4]; //number of bit to encode MVs using each of four possible tables
        int32_t AcRate[4]; //number of bit to encode AC coefficients using each of three possible table sets

        //VC1 specific
        //VLC table indexes
        int32_t qp8;
        int32_t CbpcyTableIndex;
        int32_t MvTableIndex;
        int32_t AcTableIndex;

        //trellis quant related
        bool VlcTableSearchInProgress; //true - VLC table search in progress, it is used to turn off trellis quantization, not only to speed up
                  //search but also to avoid mismatch between VLC tables that were used during trellis quantization and coding. 
        int16_t TrellisCoefficients[5][6][64]; // 5= (4 different transfroms for inter plus 8x8 intra), 6 blocks, 64 coeff in each
        int8_t RoundControl[5][6][64]; // 5= (4 different transfroms for inter plus 8x8 intra), 6 blocks, 64 coeff in each
        MeTrellisNode trellis[65][NumOfLevelInTrellis+64];

        uint8_t mvScoreBoardArr[128][64];
        uint32_t mvScoreBoardFlag;
    };
}
#endif
#endif

